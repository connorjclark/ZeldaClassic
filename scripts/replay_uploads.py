# Downloads all the replays uploaded via the replay upload service,
# copies them to a new replay file that can be updated without modifying
# the original, and enables running all of them via run_replay_tests.py.
#
# This requires an auth key placed at `~/.aws/credentials`
# (or `C:\Users\USERNAME\.aws\credentials`) under `[zc-replays]`.
# Ask Connor to get a personal key. That key only reads; writing baselines
# (--refresh_known_good / --update_known_good) takes a read/write key under
# `[zc-replays-write]`.
#
# Note this currently accepts any graphical changes, and will only report
# an error if a replay fails to update due to behavioral changes (ex: rng
# desync, not loading into a screen on the expected frame, etc).
#
# To see a list of all uploaded replays grouped by qst, run:
#
#   python scripts/replay_uploads.py
#
# To attempt to update the local copy of all replays using the current local build, run:
#
#   python scripts/replay_uploads.py --update
#
# These replays can be used to validate changes - but only by exluding the
# "known bad" replays which currently fail to update for some unknown reason.
# For example:
#
#   You have a change you want more validation for. Go to the commit just
#   before, build, and run `python scripts/replay_uploads.py --update --only_known_good`.
#   Then, with your new change, run
#   `python scripts/replay_uploads.py --assert --only_known_good`. If
#   anything fails, generate a compare report and investigate.
#
# The known good replays (scripts/replay_uploads_known_good_replays.json) are pinned
# to how much of each replay was validated, so everyone runs the same replays even
# as players upload longer versions of them. There are two ways to change that list:
#
#   python scripts/replay_uploads.py --refresh_known_good
#
# re-records the baselines of the current known good replays (ex: after a change
# that intentionally alters gameplay or graphics). Every one of them must still
# pass, otherwise nothing is written - a replay that no longer passes is a bug to
# investigate, not something to silently drop.
#
#   python scripts/replay_uploads.py --update --update_known_good
#
# rebuilds the list from scratch (picking up new and re-uploaded replays, and
# dropping any that no longer pass). Some will fail to update, that is ok. Commit
# the result either way.
#
# --update records with the JIT and ZASM optimizer disabled, so the baseline comes from
# the plain interpreter. Both of the above then assert the updated replays with both
# enabled, and only replays that pass that too are in the known good set. Replays
# that update but fail that assert are asserted again without the JIT and optimizer,
# and listed at the end as likely JIT/optimizer bugs (if they pass that) or replay
# bugs (if they don't).
#
# The baselines never contain raw key events (keys not bound to a game button): they
# are whatever the player typed while the game had focus, and the game rarely needs
# them. They are removed when a copy is made from an original, and again before every
# --update, so a quest that does need them fails to update and is not known good.
#
# Each known good replay also pins its baseline: the sha256 of the updated local copy
# that was validated. The baselines themselves live in the upload bucket under
# `baselines/<sha256>.zplay.gz`, where both of the above put them, so anyone (and CI)
# can assert exactly the baselines a commit was validated with. This is the uploaded
# replays' equivalent of the test suite's checked-in .zplay files: the json is the
# pointer, the bucket is the store. A local copy that differs from its pin is left
# alone (it may be a local --update), unless --use_pinned_baselines restores it.
#
# The exit code is non-zero when a replay fails in --assert mode, or in --update mode
# when only the known good replays were run (they should all update).
#
# To debug why a replay can't be updated, run:
#
#   python scripts/replay_uploads.py --debug 50C00A5197148F3D1587A16509DCC89F/0be80dd3-c719-4203-a314-fec7e9557a7f.zplay
#
# To view a specific replay, run:
#
#   python tests/run_replay_tests.py $PWD/.tmp/replay_uploads/D2905F6A4976CBC02C3ACC92AD6CB12C/c4c21082-dc31-49e0-9f35-d18bfb87c841-updated-main.zplay --show
#
# CI asserts the known good replays on every run (the "Replay uploads" jobs in
# .github/workflows/test.yml, Linux x64 only) with --ci, which downloads just the
# pinned baselines and qst files, and refreshes them on demand with
# .github/workflows/replay-uploads-refresh.yml. CI has no ~/.aws profile; it
# provides the bucket key as AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY instead.

import argparse
import csv
import gzip
import hashlib
import io
import json
import os
import shlex
import shutil
import subprocess
import sys

from pathlib import Path

import archives
import boto3
import botocore.exceptions
import common
import database

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent
# Resolved, so these paths compare equal to the (resolved) paths in test results.
replays_dir = (root_dir / '.tmp/replay_uploads').resolve()

os.sys.path.append(str((root_dir / 'tests/lib').absolute()))
os.sys.path.append(str((root_dir / 'tests').absolute()))
import replay_helpers

from replays import ReplayTestResults

channel = 'main'
updated_suffx = f'-updated-{channel}.zplay'
updated_suffix_no_ext = f'-updated-{channel}'

parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument(
    '--update', action='store_true', help='Update all the local replay copies'
)
parser.add_argument(
    '--assert',
    action='store_true',
    help='Assert all the local replay copies',
    dest='assertmode',
)
parser.add_argument(
    '--only_known_good',
    action='store_true',
    help='Filter out all replays that are not known to be good / update-able',
)
parser.add_argument(
    '--update_known_good',
    action='store_true',
    help='Rebuild the checked-in file for which replays are update-able from whatever passes',
)
parser.add_argument(
    '--refresh_known_good',
    action='store_true',
    help='Re-record the baselines of the known good replays. Fails if any no longer pass',
)
parser.add_argument(
    '--use_pinned_baselines',
    action='store_true',
    help='Replace any local copy of a known good replay that differs from its pinned baseline',
)
parser.add_argument(
    '--known_good_file',
    type=Path,
    default=script_dir / 'replay_uploads_known_good_replays.json',
    help='The known good replays file to read and write',
)
parser.add_argument(
    '--test_results_folder',
    type=Path,
    default=root_dir / '.tmp/replay_uploads_test_results',
    help='Where to write the test results',
)
parser.add_argument(
    '--jit',
    action=argparse.BooleanOptionalAction,
    help='Enables JIT compilation. Defaults to off for --update, on for --assert',
)
parser.add_argument(
    '--optimize-zasm',
    action=argparse.BooleanOptionalAction,
    help='Enables the ZASM optimizer. Defaults to off for --update, on for --assert',
)
parser.add_argument(
    '--build_folder', help='The folder containing the zplayer executable to run'
)
parser.add_argument(
    '--ci',
    metavar='PLATFORM',
    help='Assert the known good replays the way CI does: download only their pinned baselines and qst files (never the originals, never the rest of a quest release), and fail if any does not pass. The value is passed on to run_replay_tests.py --ci (ex: ubuntu-22.04_x64)',
)
parser.add_argument(
    '--shard',
    help='Only with --ci: run only this part of the known good replays, ex: 2/4. Shards are balanced by frame count',
)
parser.add_argument(
    '--max_duration',
    type=int,
    help='Passed on to run_replay_tests.py: stop each replay after this many (estimated) seconds',
)
parser.add_argument(
    '--extra_args',
    help='Passed on to run_replay_tests.py --extra_args: extra flags for zplayer (ex: "-replay-debug-corrupt-frame 300")',
)
parser.add_argument('--debug', help='Debug why the given replay cannot be updated')
parser.add_argument(
    '--coverage',
    action='store_true',
    help='Save CSV of replay coverage across all database quests, using uploaded replays and test replays',
)
args = parser.parse_args()
if args.refresh_known_good:
    args.update = True
    args.only_known_good = True

bucket = None
db = database.Database()

# Where the known good baselines live in the upload bucket (see the header).
baselines_prefix = 'baselines/'


def connect_bucket():
    global bucket

    bucket_name = 'zc-replays'
    profile_name = 'zc-replays'
    if profile_name in boto3.session.Session().available_profiles:
        boto3.setup_default_session(profile_name=profile_name)
    elif not os.environ.get('AWS_ACCESS_KEY_ID'):
        # CI provides the key through the environment instead of a profile.
        raise Exception(
            f'no [{profile_name}] profile in ~/.aws/credentials (see the top of this script), and no AWS_ACCESS_KEY_ID in the environment'
        )
    s3 = boto3.resource(
        's3',
        region_name='nyc3',
        endpoint_url='https://nyc3.digitaloceanspaces.com',
    )
    bucket = s3.Bucket(bucket_name)


def prepare_and_get_replay_keys():
    connect_bucket()

    get_qst_keys_by_hash = db.get_qst_keys_by_hash()
    sizes_by_key = {
        obj.key: obj.size
        for obj in bucket.objects.all()
        if not obj.key.startswith(baselines_prefix)
    }
    # Note: currently no plans to actual upload updated replays back to bucket.
    original_keys = [key for key in sizes_by_key if updated_suffx not in key]

    print('All replays downloaded from the upload bucket\n')
    print(f'Directory: {replays_dir}')
    print(f'Total: {len(original_keys)}')

    # Download everything. A replay can be re-uploaded with more frames appended
    # (same key), so also re-download any local copy whose size doesn't match.
    for key, size in sizes_by_key.items():
        # Don't clobber local update results with whatever is in the bucket.
        if updated_suffx in key:
            size = None
        download_replay(key, size)

    # Group replays by qst_hash.
    keys_by_qst_hash = {}
    for key in original_keys:
        qst_hash = key.split('/')[0]
        if qst_hash not in keys_by_qst_hash:
            keys_by_qst_hash[qst_hash] = []
        keys_by_qst_hash[qst_hash].append(key)

    # Copy qst files.
    for qst_hash in keys_by_qst_hash.keys():
        qst_key, qst_release = get_qst_keys_by_hash[qst_hash]
        db.download_release(qst_release)
        qst_path = db.download(qst_key)

        qst_replays_dir = replays_dir / qst_hash
        copied_qst_path = qst_replays_dir / f'{qst_hash}.qst'
        if not copied_qst_path.exists():
            shutil.copyfile(qst_path, copied_qst_path)

    known_good = get_known_good_replays() if args.only_known_good else {}
    for key in original_keys:
        init_updated_replay(key, known_good.get(key))

    return original_keys


def get_replay_path(key: str) -> Path:
    return replays_dir / key


def download_replay(key: str, size: int | None):
    path = get_replay_path(key)
    if path.exists():
        local_size = path.stat().st_size
        if size is None or local_size == size:
            return

        print(f'updating: {key} (re-uploaded, {local_size} -> {size} bytes)')
    else:
        print(f'downloading: {key}')

    path.parent.mkdir(exist_ok=True, parents=True)
    bucket.download_file(key, path)


def parse_meta_line(line: str) -> tuple[str, str]:
    # Not stripped: a meta line may have an empty value ("M qst_title ").
    _, meta_key, value = line.split(' ', 2)
    return meta_key, value


# Note: these avoid replay_helpers.read_replay_meta, which caches by path - but the
# files here change during a run.
def read_replay_meta(path: Path) -> dict[str, str]:
    """Reads just the leading meta lines, without loading the rest of the replay."""
    meta = {}
    with path.open(encoding='utf-8') as f:
        for line in f:
            if not line.startswith('M'):
                break
            meta_key, value = parse_meta_line(line.rstrip('\n'))
            meta[meta_key] = value
    return meta


def read_replay(path: Path) -> tuple[dict[str, str], list[str]]:
    """Returns tuple: meta, steps"""
    lines = path.read_text('utf-8').splitlines()
    meta = {}
    num_meta = 0
    while num_meta < len(lines) and lines[num_meta].startswith('M'):
        meta_key, value = parse_meta_line(lines[num_meta])
        meta[meta_key] = value
        num_meta += 1
    return meta, [l for l in lines[num_meta:] if l]


# A key bound to a game button is recorded as that button, so a raw key step is an
# unbound key: a typed name, a cheat, a system key - or anything typed into the
# wrong window. None of it belongs in a baseline.
def is_raw_key_step(step: str) -> bool:
    # Some key names have spaces ("2 PAD"), so don't count the parts.
    parts = step.split(' ')
    return parts[0] in ('D', 'U') and len(parts) >= 4 and parts[2] == 'k'


def strip_raw_key_steps(steps: list[str]) -> list[str]:
    return [s for s in steps if not is_raw_key_step(s)]


# Rewrites the replay without its raw key steps. Returns how many were removed.
def strip_raw_key_steps_from_file(path: Path) -> int:
    meta, steps = read_replay(path)
    stripped = strip_raw_key_steps(steps)
    if len(stripped) == len(steps):
        return 0
    meta['length'] = len(stripped)
    lines = [f'M {k} {v}' for k, v in meta.items()] + stripped
    path.write_text('\n'.join(lines), 'utf-8')
    return len(steps) - len(stripped)


def get_replay_pin(key: str) -> dict:
    """The length (number of steps) and frames of the local copy of an uploaded replay."""
    # The api server only accepts a replay whose length meta matches its steps, so the
    # whole replay need not be read.
    meta = read_replay_meta(get_replay_path(key))
    return {'length': int(meta['length']), 'frames': int(meta['frames'])}


def get_updated_key(key: str):
    return key.replace('.zplay', updated_suffx)


def read_updated_replay_pin(path: Path) -> dict | None:
    meta = read_replay_meta(path)
    if 'replay_upload_length' not in meta:
        return None
    return {
        'length': int(meta['replay_upload_length']),
        'frames': int(meta['replay_upload_frames']),
    }


def baseline_sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def baseline_bucket_key(sha: str) -> str:
    return f'{baselines_prefix}{sha}.zplay.gz'


# Uploads the baseline of each replay to the bucket (unless it is already there),
# and returns each replay's baseline sha. Objects are named by content, so a
# baseline is never overwritten and every version of the known good file keeps
# pointing at exactly what it was validated with.
# The bucket from connect_bucket() is read-only for most keys; this one can write.
def get_writable_bucket():
    write_profile_name = 'zc-replays-write'
    if write_profile_name in boto3.session.Session().available_profiles:
        session = boto3.session.Session(profile_name=write_profile_name)
    else:
        # CI provides a read/write key through the environment.
        session = boto3.session.Session()
    s3 = session.resource(
        's3',
        region_name='nyc3',
        endpoint_url='https://nyc3.digitaloceanspaces.com',
    )
    return s3.Bucket(bucket.name)


def upload_baselines(paths: list[Path]) -> dict[Path, str]:
    writable_bucket = get_writable_bucket()
    existing_keys = {obj.key for obj in bucket.objects.filter(Prefix=baselines_prefix)}
    shas = {}
    num_uploaded = 0
    for path in paths:
        sha = baseline_sha(path)
        shas[path] = sha
        key = baseline_bucket_key(sha)
        if key in existing_keys:
            continue

        buf = io.BytesIO()
        # mtime=0 so the same baseline always makes the same object.
        with gzip.GzipFile(fileobj=buf, mode='wb', mtime=0) as f:
            f.write(path.read_bytes())
        try:
            writable_bucket.put_object(Key=key, Body=buf.getvalue())
        except botocore.exceptions.ClientError as e:
            if e.response['Error']['Code'] == 'AccessDenied':
                raise Exception(
                    'the bucket key cannot write (see the top of this script for the [zc-replays-write] profile)'
                ) from e
            raise
        num_uploaded += 1

    print(
        f'Uploaded {num_uploaded} baselines ({len(paths) - num_uploaded} were already in the bucket)'
    )
    return shas


def download_baseline(sha: str, path: Path):
    buf = io.BytesIO()
    try:
        bucket.download_fileobj(baseline_bucket_key(sha), buf)
    except botocore.exceptions.ClientError as e:
        if e.response['Error']['Code'] == '404':
            raise Exception(
                f'baseline {sha} for {path.name} is not in the bucket (it is written by --refresh_known_good / --update_known_good)'
            )
        raise
    data = gzip.decompress(buf.getvalue())
    if hashlib.sha256(data).hexdigest() != sha:
        raise Exception(f'baseline {sha} is corrupt in the bucket')
    path.parent.mkdir(exist_ok=True, parents=True)
    path.write_bytes(data)


# Creates the local copy of an uploaded replay that gets updated / asserted.
#
# If `pin` is given, only that much of the uploaded replay is used. Replays are
# append-only (the api server rejects any re-upload that changes existing steps),
# so a pin always recreates exactly the replay that was previously validated, no
# matter how much has been uploaded since. Without a pin, the whole replay is used.
#
# The copy is only recreated (discarding local update results) when what it was
# made from changes, and only in --update mode. Recreating it otherwise would make
# --assert fail for reasons unrelated to the build being tested.
def init_updated_replay(key: str, pin: dict | None):
    original_replay_path = get_replay_path(key)
    updated_replay_path = original_replay_path.with_name(
        original_replay_path.name.replace('.zplay', updated_suffx)
    )

    if not pin:
        pin = get_replay_pin(key)
    expected_pin = {'length': pin['length'], 'frames': pin['frames']}
    rel_path = updated_replay_path.relative_to(replays_dir)

    if updated_replay_path.exists():
        current_pin = read_updated_replay_pin(updated_replay_path)
        if current_pin == expected_pin:
            check_pinned_baseline(pin, updated_replay_path)
            return

        if not args.update:
            if args.assertmode:
                print(
                    f'warning: {rel_path} was made from {current_pin}, not {expected_pin} (run with --update to recreate)'
                )
            return

        print(f'recreating: {rel_path} ({current_pin} -> {expected_pin})')
    elif pin.get('baseline'):
        # A fresh copy starts from the baseline everyone validated against,
        # not from the original (which has no baseline to assert).
        print(f'downloading baseline: {rel_path}')
        download_baseline(pin['baseline'], updated_replay_path)
        return

    meta, steps = read_replay(original_replay_path)
    if len(steps) < pin['length']:
        raise Exception(f'{key} has fewer steps than its pin: {pin}')

    qst_hash = key.split('/')[0]
    meta['debug'] = 'true'  # Not all uploaded replays are in debug mode.
    meta['qst'] = f'{qst_hash}.qst'
    meta['replay_upload_key'] = key
    meta['replay_upload_length'] = pin['length']
    meta['replay_upload_frames'] = pin['frames']
    # The pin counts the original's steps; the copy has fewer once the raw key
    # steps are gone.
    steps = strip_raw_key_steps(steps[: pin['length']])
    meta['length'] = len(steps)
    meta['frames'] = pin['frames']

    new_lines = []
    for meta_key, value in meta.items():
        new_lines.append(f'M {meta_key} {value}')
    new_lines.extend(steps)

    updated_replay_path.write_text('\n'.join(new_lines), 'utf-8')


# The local copy matches its pin, but may not be the pinned baseline (ex: a local
# --update changed it). Leave it alone unless asked to restore the pinned one.
def check_pinned_baseline(pin: dict, updated_replay_path: Path):
    sha = pin.get('baseline')
    if not sha or baseline_sha(updated_replay_path) == sha:
        return
    # About to be re-recorded anyway.
    if args.update:
        return

    rel_path = updated_replay_path.relative_to(replays_dir)
    if args.use_pinned_baselines:
        print(f'restoring baseline: {rel_path}')
        download_baseline(sha, updated_replay_path)
    elif args.assertmode:
        print(
            f'warning: {rel_path} is not the pinned baseline (run with --use_pinned_baselines to restore it)'
        )


def run_replays_process(run_args: list, test_results_folder: Path = None):
    if test_results_folder is None:
        test_results_folder = args.test_results_folder
    if test_results_folder.exists():
        shutil.rmtree(test_results_folder)
    test_results_folder.parent.mkdir(parents=True, exist_ok=True)
    # There can be hundreds of replay paths here, which blows past Windows'
    # ~32k command line limit, so pass them in a response file instead.
    args_path = test_results_folder.parent / f'{test_results_folder.name}_args.txt'
    # argparse reads the response file as utf-8.
    args_path.write_text('\n'.join(str(arg) for arg in run_args), encoding='utf-8')
    # The exit code is not checked: a failing replay is reported through
    # test_results.json, and the caller decides what to do about it.
    subprocess.run(
        [
            sys.executable,
            root_dir / 'tests/run_replay_tests.py',
            '--test_results_folder',
            test_results_folder,
            '--root_replays_folder',
            replays_dir,
            f'@{args_path}',
        ]
    )
    test_results_json = json.loads(
        (test_results_folder / 'test_results.json').read_text('utf-8')
    )
    test_results = ReplayTestResults(**test_results_json)
    return test_results


def run_replays_process_with_version(zc_version, args):
    release_platform = common.get_release_platform()
    build_folder = archives.download(zc_version, release_platform)
    if release_platform == 'mac':
        zc_app_path = next(build_folder.glob('*.app'))
        build_folder = zc_app_path / 'Contents/Resources'
    return run_replays_process([*args, '--build_folder', build_folder])


# Maps each known good replay to its pin: how much of the replay was validated
# (see init_updated_replay).
def get_known_good_replays() -> dict[str, dict]:
    return json.loads(args.known_good_file.read_text())


def run_replays():
    if args.update:
        mode = 'update'
    elif args.assertmode:
        mode = 'assert'

    known_good = get_known_good_replays() if args.only_known_good else {}
    if known_good and all(pin.get('baseline') for pin in known_good.values()):
        # Only the pinned baselines and their qst files are needed (not the
        # originals, nor whole quest releases), which is what lets a refresh
        # run in CI.
        original_keys = prepare_known_good_replays()
        if args.refresh_known_good and len(original_keys) < len(known_good):
            print(
                'Some known good replays could not be prepared, so nothing was refreshed'
            )
            sys.exit(1)
    else:
        original_keys = prepare_and_get_replay_keys()
        exclude_keys = [
            # This replay has an RNG desync even when played in the same release it was made in.
            # Warrants further investigation.
            'DFA6082440977B8C2C0BD323D17068C4/f064251f-0507-44ef-8cc3-42c0c71dd979.zplay'
        ]
        original_keys = [key for key in original_keys if key not in exclude_keys]
        if args.only_known_good:
            original_keys = [k for k in original_keys if k in known_good]
    updated_keys = [get_updated_key(key) for key in original_keys]
    updated_paths = [get_replay_path(key) for key in updated_keys]

    # Baselines are recorded with the plain interpreter, so a JIT or optimizer bug can't
    # end up in them.
    use_jit = args.jit if args.jit is not None else mode == 'assert'
    use_optimize_zasm = (
        args.optimize_zasm if args.optimize_zasm is not None else mode == 'assert'
    )
    build_args = ['--build_folder', args.build_folder] if args.build_folder else []

    run_args = [f'--{mode}', *build_args]
    if not use_jit:
        run_args.append('--no-jit')
    if not use_optimize_zasm:
        run_args.append('--no-optimize-zasm')
    if args.extra_args:
        run_args.append(f'--extra_args={args.extra_args}')
    if args.max_duration:
        run_args += ['--max_duration', str(args.max_duration)]
    # With no paths, run_replay_tests.py would run every replay in replays_dir instead.
    if not updated_paths:
        print('No replays to run')
        sys.exit(1)

    if args.update:
        num_stripped = [strip_raw_key_steps_from_file(p) for p in updated_paths]
        num_files = sum(1 for n in num_stripped if n)
        if num_files:
            print(
                f'Removed {sum(num_stripped)} raw key steps from {num_files} replays before updating',
                flush=True,
            )

    test_results = run_replays_process([*run_args, *updated_paths])
    outcomes = get_run_outcomes(test_results)
    # Only replays that ran count: a shard runs a subset of the paths.
    failing_paths = [p for p in updated_paths if p in outcomes and not outcomes[p]]
    print_failing_replays(
        'These replays failed (to debug, use `--debug replay/path.zplay`):',
        failing_paths,
    )

    if args.update_known_good or args.refresh_known_good:
        passed_paths = [p for p in updated_paths if outcomes.get(p)]
        jit_passed = assert_with_jit(passed_paths, build_args)

        if args.refresh_known_good:
            dropped_paths = [p for p in updated_paths if p not in jit_passed]
            if dropped_paths:
                print_failing_replays(
                    'These known good replays no longer pass, so the known good replays were not refreshed (investigate, or drop them with `--update --update_known_good`):',
                    dropped_paths,
                )
                sys.exit(1)

        write_known_good(original_keys, updated_paths, jit_passed)
    elif failing_paths:
        # Plain --update over every uploaded replay is expected to have failures
        # (the ones that are not known good), so that is not an error.
        if mode == 'assert' or args.only_known_good:
            sys.exit(1)
    else:
        print('All replays passed')


# Asserts the given (freshly updated) replays with the JIT and ZASM optimizer
# enabled, and returns the ones that pass. Only those can be trusted as known good.
def assert_with_jit(updated_ok_paths: list[Path], build_args: list[str]) -> set[Path]:
    jit_passed = set()
    # With no paths, run_replay_tests.py would run every test replay instead.
    if updated_ok_paths:
        jit_test_results = run_replays_process(
            ['--assert', '--no_report_on_failure', *build_args, *updated_ok_paths],
            args.test_results_folder.with_name(f'{args.test_results_folder.name}_jit'),
        )
        jit_passed = get_passed_paths(jit_test_results)
    jit_failing_paths = [p for p in updated_ok_paths if p not in jit_passed]

    # Assert those again without the JIT and optimizer, to tell apart JIT or
    # optimizer bugs from replays that just don't assert right after updating.
    nojit_passed = set()
    if jit_failing_paths:
        nojit_test_results = run_replays_process(
            [
                '--assert',
                '--no_report_on_failure',
                *build_args,
                '--no-jit',
                '--no-optimize-zasm',
                *jit_failing_paths,
            ],
            args.test_results_folder.with_name(
                f'{args.test_results_folder.name}_nojit'
            ),
        )
        nojit_passed = get_passed_paths(nojit_test_results)
    print_failing_replays(
        'These replays updated, but then failed with the JIT and ZASM optimizer enabled, and passed without them (likely a JIT or optimizer bug):',
        [p for p in jit_failing_paths if p in nojit_passed],
    )
    print_failing_replays(
        'These replays updated, but then failed to assert even without the JIT and ZASM optimizer (likely a replay bug):',
        [p for p in jit_failing_paths if p not in nojit_passed],
    )
    return jit_passed


def write_known_good(
    original_keys: list[str], updated_paths: list[Path], good_paths: set[Path]
):
    good_paths = [p for p in updated_paths if p in good_paths]
    # Nothing passing means something is broken (ex: the build), not that every
    # replay is bad - so keep the existing list.
    if not good_paths:
        print('No replays passed, so not updating the known good replays')
        sys.exit(1)

    # Before the file is written, so it never points at a missing baseline.
    shas = upload_baselines(good_paths)

    known_good = {}
    for index, path in enumerate(updated_paths):
        if path in shas:
            known_good[original_keys[index]] = {
                **read_updated_replay_pin(path),
                'baseline': shas[path],
            }
    known_good = dict(sorted(known_good.items()))
    args.known_good_file.write_text(json.dumps(known_good, indent=2))
    print(f'Wrote {len(known_good)} known good replays to {args.known_good_file}')


# The success of every replay that ran, by path.
def get_run_outcomes(test_results: ReplayTestResults) -> dict[Path, bool]:
    # A replay may have been retried; its last run is the one that counts.
    success_by_path = {}
    for runs in test_results.runs:
        for run in runs:
            success_by_path[Path(run.path).resolve()] = run.success
    return success_by_path


def get_passed_paths(test_results: ReplayTestResults) -> set[Path]:
    return {p for p, success in get_run_outcomes(test_results).items() if success}


# Gets just what asserting the known good replays needs: each one's pinned
# baseline, and its qst. Unlike prepare_and_get_replay_keys, this never lists
# or downloads the originals (there is nothing to assert in them), and takes
# only the qst from the quest's release (not its music etc). Returns the keys
# of the replays that are ready to run.
def prepare_known_good_replays(wanted_keys: list[str] = None) -> list[str]:
    connect_bucket()
    known_good = get_known_good_replays()
    if wanted_keys is None:
        wanted_keys = list(known_good)
    qst_keys_by_hash = db.get_qst_keys_by_hash()

    keys = []
    unavailable_qst_hashes = set()
    for key in wanted_keys:
        pin = known_good[key]
        if not pin.get('baseline'):
            raise Exception(
                f'{key} has no pinned baseline (run --refresh_known_good first)'
            )

        qst_hash = key.split('/')[0]
        if qst_hash in unavailable_qst_hashes:
            continue
        copied_qst_path = replays_dir / qst_hash / f'{qst_hash}.qst'
        if not copied_qst_path.exists():
            if qst_hash not in qst_keys_by_hash:
                print(f'warning: no quest {qst_hash} in the database, skipping {key}')
                unavailable_qst_hashes.add(qst_hash)
                continue
            qst_key, _ = qst_keys_by_hash[qst_hash]
            qst_path = db.download(qst_key)
            # The hash is the qst's md5, so this catches a changed release too.
            if hashlib.md5(qst_path.read_bytes()).hexdigest().upper() != qst_hash:
                print(f'warning: {qst_key} does not hash to {qst_hash}, skipping {key}')
                unavailable_qst_hashes.add(qst_hash)
                continue
            copied_qst_path.parent.mkdir(exist_ok=True, parents=True)
            shutil.copyfile(qst_path, copied_qst_path)

        updated_path = get_replay_path(get_updated_key(key))
        if not updated_path.exists():
            print(f'downloading baseline: {key}')
            download_baseline(pin['baseline'], updated_path)
        elif args.ci or args.refresh_known_good:
            # Both must start from exactly the pinned baseline, whatever a local
            # --update left here.
            if baseline_sha(updated_path) != pin['baseline']:
                print(f'downloading baseline: {key}')
                download_baseline(pin['baseline'], updated_path)
        else:
            # A local copy that differs is probably a local --update, kept for
            # the --assert that follows it (see the top of this file).
            check_pinned_baseline(pin, updated_path)
        keys.append(key)

    return keys


# Splits the known good replays into `count` shards balanced by frame count,
# and returns the one at `index` (1-based). Same greedy packing as the test
# suite uses: largest first, each into the lightest shard so far.
def get_shard(keys: list[str], known_good: dict, shard: str) -> list[str]:
    index, count = (int(s) for s in shard.split('/'))
    if index > count or index <= 0 or count <= 0:
        raise ValueError('invalid --shard')

    def cost(key: str) -> int:
        frames = known_good[key]['frames']
        if args.max_duration:
            # run_replay_tests.py stops a replay at its estimated fps (1500 for
            # any quest it does not know) times the max duration.
            frames = min(frames, 1500 * args.max_duration)
        return frames

    shards = [[] for _ in range(count)]
    loads = [0] * count
    for key in sorted(keys, key=lambda k: -cost(k)):
        i = loads.index(min(loads))
        shards[i].append(key)
        loads[i] += cost(key)
    return shards[index - 1]


def run_ci():
    known_good = get_known_good_replays()
    keys = list(known_good)
    # Shard first, so each shard downloads only what it runs.
    if args.shard:
        keys = get_shard(keys, known_good, args.shard)
    wanted_keys = keys
    keys = prepare_known_good_replays(keys)
    # A known good replay whose quest went missing from the database would
    # otherwise drop out of CI without anyone noticing.
    if len(keys) < len(wanted_keys):
        print(
            f'{len(wanted_keys) - len(keys)} known good replays could not be prepared (see the warnings above)'
        )
        sys.exit(1)
    paths = [get_replay_path(get_updated_key(key)) for key in keys]
    if not paths:
        print('No replays to run')
        sys.exit(1)

    run_args = ['--assert', '--ci', args.ci, '--retries=1', '--prune_test_results']
    if args.build_folder:
        run_args += ['--build_folder', args.build_folder]
    if args.max_duration:
        run_args += ['--max_duration', str(args.max_duration)]
    if args.extra_args:
        run_args.append(f'--extra_args={args.extra_args}')
    test_results = run_replays_process([*run_args, *paths])

    # Here a replay that did not run at all is a failure too.
    outcomes = get_run_outcomes(test_results)
    failing_paths = [p for p in paths if not outcomes.get(p)]
    print_failing_replays(
        'These replays failed (to debug, use `--debug replay/path.zplay`):',
        failing_paths,
    )
    if failing_paths:
        sys.exit(1)
    print('All replays passed')


def print_failing_replays(header: str, paths: list[Path]):
    if not paths:
        return

    print(f'{header}\n')
    for path in paths:
        meta = replay_helpers.read_replay_meta(path)
        title = meta.get('qst_title', meta['qst'])
        print(path.relative_to(replays_dir))
        print(title)
        print()


# https://docs.google.com/spreadsheets/d/1BD0wsO2fDpvZDMgI9-wwgcLKNqCc7j8e8ZLJlfNdTz8/edit?gid=0#gid=0&fvid=2018909849
def make_coverage_csv():
    known_good = get_known_good_replays()
    original_keys = prepare_and_get_replay_keys()

    replays_by_qst_hash = {}
    for key in original_keys:
        path = get_replay_path(key)
        meta = replay_helpers.read_replay_meta(path)

        qst_hash = meta['qst_hash']
        if qst_hash not in replays_by_qst_hash:
            replays_by_qst_hash[qst_hash] = []
        replays_by_qst_hash[qst_hash].append(path)

    quests = db.quests
    quests.sort(
        key=lambda x: x.rating_score if x.rating_score != None else -1, reverse=True
    )

    test_replay_datas = []
    for path in (root_dir / 'tests/replays').rglob('*.zplay'):
        if 'playground' in str(path):
            continue
        if 'z3' in str(path):
            continue
        if 'misc' in str(path):
            continue

        meta = replay_helpers.read_replay_meta(path)
        qst_hash = meta.get('qst_hash')
        title = meta.get('qst_title')

        if 'nargads_trail_crystal_crusades' in str(path):
            title = "Nargad's Trail: Crystal Crusades"
        if 'demosp' in str(path):
            title = 'Demo Quest SP'
        if 'new2013' in str(path):
            title = 'New Quest 2013'

        d = next(
            (
                d
                for d in test_replay_datas
                if (title and d['title'] == title)
                or (qst_hash and d['hash'] == qst_hash)
            ),
            None,
        )
        if not d:
            d = {'title': title, 'hash': qst_hash, 'paths': []}
            test_replay_datas.append(d)
        d['paths'].append(path)

    rows = []
    for quest in quests:
        replay_paths = []
        hashes = []

        for rls in quest.releases:
            if not rls.resource_hashes:
                continue

            for i, r in enumerate(rls.resources):
                if r.lower().endswith('.qst'):
                    h = rls.resource_hashes[i]
                    paths = replays_by_qst_hash.get(h, [])
                    replay_paths.extend(paths)
                    hashes.append(h)

        replay_paths = set(replay_paths)

        frames = 0
        frames_good = 0
        for path in replay_paths:
            meta = replay_helpers.read_replay_meta(path)
            frames += meta['frames']
            if str(path.relative_to(replays_dir)) in known_good:
                frames_good += meta['frames']

        test_replay_data = next(
            (
                d
                for d in test_replay_datas
                if d['title'] == quest.name or d['hash'] in hashes
            ),
            None,
        )
        if test_replay_data:
            for path in test_replay_data['paths']:
                meta = replay_helpers.read_replay_meta(path)
                frames += meta['frames']
                frames_good += meta['frames']
            test_replay_datas.remove(test_replay_data)

        version = quest.data.get('zcVersion', '')
        if version.startswith('1.92'):
            version = '1.92'
        elif version.startswith('2.50'):
            version = '2.50'

        url = quest.data.get('projectUrl', '')
        frames_minutes = '{0:.1f}'.format(frames / 60 / 60)
        frames_good_minutes = '{0:.1f}'.format(frames_good / 60 / 60)
        rows.append(
            [
                quest.name,
                url,
                version,
                len(replay_paths),
                frames_minutes,
                frames_good_minutes,
            ]
        )

    if test_replay_datas:
        print('warning: unused test replays')
        for d in test_replay_datas:
            print(d)

    path = root_dir / '.tmp/replay_coverage.csv'
    with open(path, 'w', newline='') as csvfile:
        w = csv.writer(csvfile, quoting=csv.QUOTE_MINIMAL)
        w.writerow(
            [
                'Quest',
                'URL',
                'ZC Version',
                '# Replays',
                'Minutes (total)',
                'Minutes (good)',
            ]
        )
        w.writerows(rows)
        print(f'saved: {path}')


def print_info():
    original_keys = prepare_and_get_replay_keys()

    replays_by_qst_hash = {}
    frames = 0
    for key in original_keys:
        path = get_replay_path(key)
        meta = replay_helpers.read_replay_meta(path)
        frames += meta['frames']

        qst_hash = meta['qst_hash']
        if qst_hash not in replays_by_qst_hash:
            replays_by_qst_hash[qst_hash] = []
        replays_by_qst_hash[qst_hash].append(path)

    print(f'Frames: {frames}')
    print('Hours: {0:.1f}'.format(frames / 60 / 60 / 60))
    print()

    for qst_hash, paths in replays_by_qst_hash.items():
        paths.sort(key=lambda p: -replay_helpers.read_replay_meta(p)['frames'])

        title = replay_helpers.read_replay_meta(paths[0]).get('qst_title', qst_hash)
        print(f'{title.strip()} ({len(paths)})')
        for path in paths:
            meta = replay_helpers.read_replay_meta(path)
            frames = meta['frames']
            print(f'\t{path.relative_to(replays_dir)} ({frames} frames)')
        print()


# Finds the most recent ZC version that can update the replay, does that update, and prints a
# command to run to see a compare report against the current local build.
def debug_replay(replay_path: Path):
    if updated_suffix_no_ext not in replay_path.name:
        replay_path = replay_path.with_stem(
            f'{replay_path.stem}{updated_suffix_no_ext}'
        )

    replay_path = replays_dir / replay_path
    meta = replay_helpers.read_replay_meta(replay_path)
    title = meta.get('qst_title', meta['qst'])
    zc_version_updated = meta['zc_version_updated']
    print(f'title: {title}')
    print(f'zc_version_updated: {zc_version_updated}')

    tmp_path = replay_path.with_stem(f'{replay_path.stem}-tmp')
    shutil.copyfile(replay_path, tmp_path)

    # Same as --update: the baseline is made without the JIT or optimizer.
    update_args = ['--update', '--no-jit', '--no-optimize-zasm']

    # Validate that this replay can even be updated in the version that created it.
    test_results = run_replays_process_with_version(
        zc_version_updated, [tmp_path, *update_args]
    )
    if not test_results.runs[-1][0].success:
        print('cannot update the replay even in the same ZC version it was created in')
        tmp_path.unlink()
        sys.exit(1)

    shutil.copyfile(replay_path, tmp_path)
    test_results = run_replays_process([tmp_path, *update_args])
    if test_results.runs[-1][0].success:
        print(test_results.runs[-1][0])
        print(
            'was able to update the replay with the current build - nothing to debug.'
        )
        tmp_path.unlink()
        sys.exit()

    # Reduce the replay to just the part that is failing.
    stopped_frame = test_results.runs[-1][0].frame
    lines = tmp_path.read_text().splitlines()
    for i, line in enumerate(lines):
        type, frame, _ = line.split(' ', 2)
        if type == 'M':
            continue

        frame = int(frame)
        # Go 1s beyond where it fails.
        if frame > stopped_frame + 60:
            lines = lines[: i + 1]
            break

    tmp_trimmed_path = replay_path.with_stem(f'{replay_path.stem}-tmp-trimmed')
    tmp_trimmed_path.write_text('\n'.join(lines))
    shutil.copyfile(tmp_trimmed_path, tmp_path)

    # Bisect to find the newest version that can update this replay.

    # First, assume and validate the good/bad versions.
    if zc_version_updated == '2.55.7':
        good_version = '3.0.0-prerelease.76+2024-10-19'
    elif zc_version_updated == '2.55.8':
        good_version = '3.0.0-prerelease.85+2025-01-10'
    elif zc_version_updated == '2.55.9':
        good_version = '3.0.0-prerelease.95+2025-03-25'
    else:
        good_version = '3.0.0-prerelease.1+2023-11-12'
    bad_version = archives.get_revisions(common.get_release_platform(), 'main')[-1].tag
    print(f'assuming good: {good_version}, bad: {bad_version}')

    print('Running bisect...')
    output = subprocess.check_output(
        [
            sys.executable,
            root_dir / 'scripts/bisect_builds.py',
            '--good',
            good_version,
            '--bad',
            bad_version,
            '--check_return_code',
            '--',
            'bash',
            '-c',
            f'cp {tmp_trimmed_path} {tmp_path} && %zc -v0 -headless -replay-exit-when-done -no-jit -no-optimize-zasm -update {tmp_path}',
        ],
        encoding='utf-8',
        stderr=subprocess.STDOUT,
    )

    bisect_done = False
    last_good_version = None
    for line in output.splitlines():
        print(line)
        if bisect_done:
            # print(line)
            if line.startswith('GOOD'):
                last_good_version = line[4:].strip()
        elif line.strip() == 'bisect finished!':
            bisect_done = True

    print('\nBisect done.')
    print(f'Last version that can update replay: {last_good_version}')

    shutil.copyfile(tmp_trimmed_path, tmp_path)
    test_results = run_replays_process_with_version(
        last_good_version, [tmp_path, *update_args]
    )
    if not test_results.runs[-1][0].success:
        print(
            f'was unable to update the replay with the calculated "last good" version: {last_good_version}.'
        )
        tmp_path.unlink()
        tmp_trimmed_path.unlink()
        sys.exit(1)

    debug_replay_path = replay_path.with_stem(f'{replay_path.stem}-debug')
    shutil.copyfile(tmp_path, debug_replay_path)
    print(f'Replayed updated at: {debug_replay_path}')

    print('\nTo see a compare report, run:')
    print(
        f'  python tests/run_replay_tests.py --baseline_version {last_good_version} {debug_replay_path}'
    )

    tmp_path.unlink()
    tmp_trimmed_path.unlink()


if args.coverage:
    make_coverage_csv()
elif args.ci:
    run_ci()
elif args.update or args.assertmode:
    run_replays()
elif args.debug:
    debug_replay(Path(args.debug))
else:
    print_info()
    print('exiting (use --assert or --update to run replays)')
    sys.exit()
