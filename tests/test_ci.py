import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

from collections import namedtuple
from pathlib import Path

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent
tmp_dir = root_dir / '.tmp/test_ci'

sys.path.append(str((root_dir / 'scripts').absolute()))
import run_target

CI = 'CI' in os.environ
is_mac = platform.system() == 'Darwin'
is_windows = platform.system() == 'Windows'
is_linux = platform.system() == 'Linux'


class TestCi(unittest.TestCase):
    def setUp(self):
        self.maxDiff = None
        self.build_folder = run_target.get_build_folder()
        self.ci_script = root_dir / '.github' / 'workflows' / 'ci.py'

        if tmp_dir.exists():
            shutil.rmtree(tmp_dir)
        tmp_dir.mkdir(parents=True)

    def run_ci(self, args: list, env=None):
        cmd = [sys.executable, str(self.ci_script)] + args
        print(f"Running: {' '.join(cmd)}")
        return subprocess.check_call(cmd, env=env)

    def test_ci(self):
        if 'CI' in os.environ:
            self.skipTest('this test is only for local development')

        packages_dir = tmp_dir / 'packages'
        extracted_dir = tmp_dir / 'extracted-package'

        self.run_ci(
            ['package', '--repo', 'dummy/repo', '--packages-dir', str(packages_dir)],
        )

        package = next(packages_dir.glob('*.*'))
        print(f'extracting package: {package}')

        self.run_ci(
            [
                'extract-package',
                '--repo',
                'dummy/repo',
                '--package',
                str(package),
                '--build-folder',
                str(extracted_dir),
            ],
        )

        print('running replay tests')

        self.run_ci(
            [
                'replay-tests',
                '--repo',
                'dummy/repo',
                '--build-folder',
                str(extracted_dir),
                '--extra-args=--filter=circle.zplay',
            ],
        )

        # TODO: add a test for the compare command


class TestReplayReport(unittest.TestCase):
    """The helpers behind `ci.py report-replays` and `refresh-replays`, on
    hand-made jobs and test results."""

    @classmethod
    def setUpClass(cls):
        sys.path.append(str((root_dir / '.github/workflows').absolute()))
        import ci

        cls.ci = ci

    def setUp(self):
        self.maxDiff = None
        self.work_dir = Path(tempfile.mkdtemp(prefix='test_ci_replays_'))

    def tearDown(self):
        shutil.rmtree(self.work_dir)

    def write_results(self, artifact: str, run_dir: str, results: dict):
        path = self.work_dir / artifact / 'test_results' / run_dir / 'test_results.json'
        path.parent.mkdir(parents=True)
        path.write_text(json.dumps(results), 'utf-8')
        return path

    def test_replay_job_outcomes(self):
        Job = namedtuple('Job', 'name conclusion')
        linux = 'test (ubuntu-22.04, x64, clang, 4, RelWithDebInfo, true) / '
        windows = 'test (windows-2022, x64, msvc, 5, Release) / '
        jobs = [
            Job(linux + 'Replays 1/4', 'success'),
            Job(linux + 'Replay uploads 1/4', 'success'),
            Job(windows + 'Replays 5/5', 'success'),
            # The platforms without uploaded replays list the job skipped,
            # with its matrix name unexpanded.
            Job(windows + 'Replay uploads ${{ matrix.shard_index }}/4', 'skipped'),
            Job('Replay report', 'failure'),
            Job('web', 'failure'),
        ]
        outcomes = self.ci._replay_job_outcomes
        self.assertEqual(outcomes(jobs), ('success', 'success'))

        jobs[2] = jobs[2]._replace(conclusion='failure')
        self.assertEqual(outcomes(jobs), ('failure', 'success'))

        jobs[1] = jobs[1]._replace(conclusion='cancelled')
        self.assertEqual(outcomes(jobs), ('failure', 'other'))

        self.assertEqual(outcomes([jobs[0], jobs[3]]), ('success', 'none'))
        self.assertEqual(outcomes([]), ('none', 'none'))

    def test_summarize_failing_suite_replays(self):
        def run(name, **failure):
            return {'name': name, 'success': not failure, **failure}

        gcc = 'replays-ubuntu-22.04-x64-gcc-1-of-1'
        msvc = 'replays-windows-2022-x64-msvc-5-of-5'
        linux = {'runs_on': 'ubuntu-22.04', 'arch': 'x64'}
        self.write_results(
            gcc,
            '100',
            {
                **linux,
                'runs': [
                    [run('kriztles_3.zplay', failing_frame=40), run('circle.zplay')],
                ],
            },
        )
        self.write_results(
            gcc,
            '100-nojit',
            {**linux, 'runs': [[run('kriztles_3.zplay', failing_frame=41)]]},
        )
        self.write_results(
            msvc,
            '100',
            {
                'runs_on': 'windows-2022',
                'arch': 'x64',
                # Two passes: only the last one counts.
                'runs': [
                    [run('circle.zplay', failing_frame=1)],
                    [run('circle.zplay'), run('zc_250.zplay', exit_code=-11)],
                ],
            },
        )
        # A summary artifact beside them is not a shard.
        (self.work_dir / 'replay-uploads-summary-1-of-4').mkdir()

        self.assertEqual(
            self.ci._summarize_failing_suite_replays(self.work_dir),
            [
                {
                    'name': 'kriztles_3.zplay',
                    'platforms': ['ubuntu-22.04-x64-gcc', 'ubuntu-22.04-x64-gcc (no JIT)'],
                    'how': 'differs at frame 40',
                },
                {
                    'name': 'zc_250.zplay',
                    'platforms': ['windows-2022-x64-msvc'],
                    'how': 'crashed (exit code -11)',
                },
            ],
        )

    def test_sort_uploads_results_by_side(self):
        shard1 = 'replay-uploads-ubuntu-22.04-x64-clang-1-of-4'
        shard3 = 'replay-uploads-ubuntu-22.04-x64-clang-3-of-4'
        for artifact, sides in [(shard1, ['current', 'baseline']), (shard3, ['current'])]:
            for side in sides:
                rel = (
                    'replay_uploads_test_results'
                    if side == 'current'
                    else 'replay_uploads_baseline/test_results'
                )
                path = self.work_dir / artifact / rel / 'test_results.json'
                path.parent.mkdir(parents=True)
                path.write_text(json.dumps({'side': side}), 'utf-8')
                (path.parent / 'snapshot.png').write_bytes(b'')
        summary = self.work_dir / 'replay-uploads-summary-1-of-4/shard.json'
        summary.parent.mkdir()
        summary.write_text('{}', 'utf-8')

        work = self.work_dir / 'work'
        self.ci._sort_uploads_results_by_side(self.work_dir, work)

        self.assertEqual(
            sorted(str(p.relative_to(work)) for p in work.rglob('*') if p.is_file()),
            [
                f'baseline/{shard1}/snapshot.png',
                f'baseline/{shard1}/test_results.json',
                f'current/{shard1}/snapshot.png',
                f'current/{shard1}/test_results.json',
                f'current/{shard3}/snapshot.png',
                f'current/{shard3}/test_results.json',
            ],
        )
        for side in ['baseline', 'current']:
            results = json.loads((work / side / shard1 / 'test_results.json').read_text('utf-8'))
            self.assertEqual(results, {'side': side})
        self.assertTrue(summary.exists())

    def test_table_cell(self):
        self.assertEqual(
            self.ci._table_cell('CHECK failed at x.cpp:1 f: a || b'),
            'CHECK failed at x.cpp:1 f: a \\|\\| b',
        )


if __name__ == '__main__':
    unittest.main()
