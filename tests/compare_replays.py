# Given various sources of snapshots (either from local filesystem, or
# downloaded from a GitHub Actiosn workflow run), generates an HTML report
# presenting the snapshots frame-by-frame, and their differences.
#
# You must run a local web server for the HTML to work:
#     cd tests/compare-tracks
#     python -m http.server 8000

# TODO: many things could use better names!

import argparse
from argparse import ArgumentTypeError
import requests
import io
import os
import re
import json
import zipfile
import shutil
import json
from pathlib import Path
from itertools import groupby
from github import Github


def dir_path(path):
    if os.path.isdir(path):
        return Path(path)
    else:
        raise ArgumentTypeError(f'{path} is not a valid directory')


parser = argparse.ArgumentParser()
parser.add_argument('--workflow_run', type=int, action='append')
parser.add_argument('--local', type=dir_path, action='append')
parser.add_argument('--repo', default='ArmageddonGames/ZQuestClassic')
parser.add_argument('--token')
args = parser.parse_args()

# See:
# - https://nightly.link/
# - https://github.com/actions/upload-artifact/issues/51
if args.workflow_run and not args.token:
    print('no token detected, will download artifacts via nightly.link')


if not args.workflow_run and not args.local:
    raise ArgumentTypeError(
        'must provide at least one --workflow_run or --local')


gh = Github(args.token)
repo = gh.get_repo(args.repo)
script_dir = os.path.dirname(os.path.realpath(__file__))
tracks_dir = f'{script_dir}/compare-tracks'
out_dir = Path(f'{script_dir}/compare-report')
num_collections = 0


def download_artifact(artifact, dest):
    if args.token:
        r = requests.get(artifact.archive_download_url, headers={
            'authorization': f'Bearer {args.token}',
        })
    else:
        url = f'https://nightly.link/{args.repo}/actions/artifacts/{artifact.id}.zip'
        r = requests.get(url)

    zip = zipfile.ZipFile(io.BytesIO(r.content))
    zip.extractall(dest)
    zip.close()


def get_replay_from_snapshot_path(path):
    return path.name[:path.name.index('.zplay') + len('.zplay')]


# TODO: better name for this
# An entry in `replay_data` represents:
#  - an individual .zplay run and its snapshots
#  - what platform it ran on
#  - a label
def collect_replay_data_from_directory(directory: Path):
    local_replay_data = []

    all_snapshot_paths = sorted(directory.rglob('*.zplay*.png'))

    # `directory` could have many separate replay test runs from different
    # platforms; and from sharded runs on the same platform. The platform
    # for the snapshots are denoted by the `test_results.json` file in the
    # same directory. Use that information to collect replay data.
    snapshot_paths_by_source = {}
    for test_results_path in directory.rglob('test_results.json'):
        test_results = json.loads(test_results_path.read_text())
        test_run_dir = test_results_path.parent
        snapshot_paths = sorted(test_run_dir.rglob('*.zplay*.png'))
        if len(snapshot_paths) == 0:
            print(f'{test_run_dir.relative_to(directory)} has no snapshots, ignoring')
            continue

        if 'ci' in test_results:
            source = test_results['ci']
        else:
            print(f'{test_results_path.relative_to(directory)} missing ci property')
            source = '?'

        if source not in snapshot_paths_by_source:
            snapshot_paths_by_source[source] = []
        snapshot_paths_by_source[source].extend(snapshot_paths)

    for source, snapshot_paths in snapshot_paths_by_source.items():
        print(source, len(list(snapshot_paths)))

    for test_results_path in directory.rglob('test_results.json'):
        print(test_results_path)
        test_run_dir = test_results_path.parent

        snapshot_paths = sorted(test_run_dir.rglob('*.zplay*.png'))
        if len(snapshot_paths) == 0:
            raise Exception(
                f'{test_run_dir.relative_to(directory)} has no snapshots')

        test_results = json.loads(test_results_path.read_text())
        if 'runs_on' not in test_results:
            raise Exception(
                f'{test_run_dir.relative_to(directory)}: expected property runs_on')

        runs_on = test_results['runs_on']
        arch = test_results['arch']

        # TODO rename label
        source = test_run_dir.relative_to(directory)

        for replay, snapshots in groupby(snapshot_paths, get_replay_from_snapshot_path):
            local_replay_data.append({
                'replay': replay,
                'snapshots': [{
                    'path': s,
                    'frame': int(re.match(r'.*\.zplay\.(\d+)', s.name).group(1)),
                    'unexpected': 'unexpected' in s.name,
                } for s in snapshots],
                'source': source,
                'runs_on': runs_on,
                'arch': arch,
            })
            local_replay_data[-1]['snapshots'].sort(key=lambda s: s['frame'])

    print(local_replay_data)

    # exit()

    # all_snapshots = sorted(directory.rglob('*.zplay*.png'))
    # for replay, snapshots in groupby(all_snapshots, get_replay_from_snapshot_path):
    #     replay_data.append({
    #         'replay': replay,
    #         'snapshots': [{
    #             'url': str(s.relative_to(tracks_dir)),
    #             'frame': int(re.match(r'.*\.zplay\.(\d+)', s.name).group(1)),
    #             'unexpected': 'unexpected' in s.name,
    #         } for s in snapshots],
    #         'source': str(directory.relative_to(tracks_dir)),
    #     })
    #     replay_data[-1]['snapshots'].sort(key=lambda s: s['frame'])

    return local_replay_data


def collect_replay_data_from_workflow_run(run_id):
    run_replay_data = []
    workflow_dir = Path(f'{tracks_dir}/gha-{run_id}')
    run = repo.get_workflow_run(run_id)

    for artifact in run.get_artifacts():
        if artifact.name.startswith('replays-'):
            # strip 'x-of-y'
            # name_without_part = re.match(
            #     r'(.*)-\d+-of-\d+', artifact.name).group(1)
            # dest = workflow_dir / name_without_part
            dest = workflow_dir / artifact.name
            dest.mkdir(parents=True, exist_ok=True)
            if next(dest.glob('*'), None) is None:
                download_artifact(artifact, dest)
            run_replay_data.extend(collect_replay_data_from_directory(dest))

    return run_replay_data


if out_dir.exists():
    shutil.rmtree(out_dir)
out_dir.mkdir(parents=True)

# TODO: push args.* to same array in argparse so that order is preserved.
# first should always be baseline. For now, assume it is the workflow option.

all_replay_data = []
if args.workflow_run:
    for run_id in args.workflow_run:
        replay_data = collect_replay_data_from_workflow_run(run_id)
        all_replay_data.extend(replay_data)
        print(
            f'found {len(replay_data)} replay data from workflow run {run_id}')
if args.local:
    local_index = 0
    for directory in args.local:
        # dest = Path(f'{tracks_dir}/local-{local_index}')
        # if dest.exists():
        #     shutil.rmtree(dest)
        # dest.mkdir(parents=True)
        # for file in directory.rglob('*.zplay*.png'):
        #     shutil.copy(file, dest)
        replay_data = collect_replay_data_from_directory(directory)
        all_replay_data.extend(replay_data)
        local_index += 1
        print(f'found {len(replay_data)} replay data from {directory}')


for replay_data in all_replay_data:
    for snapshot in replay_data['snapshots']:
        print(snapshot['path'], snapshot['path'].absolute())
        dest = out_dir / snapshot['path']
        shutil.copy2(snapshot['path'].absolute(), dest)

# exit()

html = Path(f'{script_dir}/compare-resources/compare.html').read_text('utf-8')
css = Path(f'{script_dir}/compare-resources/compare.css').read_text('utf-8')
js = Path(f'{script_dir}/compare-resources/compare.js').read_text('utf-8')
deps = Path(f'{script_dir}/compare-resources/pixelmatch.js').read_text('utf-8')

result = html.replace(
    '// JAVASCRIPT', f'const data = {json.dumps(all_replay_data, indent=2)}\n  {js}')
result = result.replace('// DEPS', deps)
result = result.replace('/* CSS */', css)
out_path = Path(f'{out_dir}/index.html')
out_path.write_text(result)
print(f'report written to {out_path}')
