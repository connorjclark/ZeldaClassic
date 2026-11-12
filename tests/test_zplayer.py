# Tests ZPlayer.
#
# To run:
#
#   python tests/test_zplayer.py

import os
import platform
import shutil
import sys
import tempfile
import unittest

from pathlib import Path

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent

sys.path.append(str((root_dir / 'scripts').absolute()))
import run_target

CI = 'CI' in os.environ
is_mac = platform.system() == 'Darwin'


class TestZPlayer(unittest.TestCase):
    def setUp(self):
        self.maxDiff = None

    def test_zplayer(self):
        run_target.check_run(
            'zplayer',
            ['-test-zc', str(root_dir / 'tests')],
        )

    def test_compat_rules_survive_junk_rule_wipe(self):
        # readrules zeroes a large range of the rule array for quests saved
        # before compatrule_version 27, so a compat rule set for old quests must
        # be applied after that wipe or it silently never takes effect. Load real
        # pre-2.55 quests and check that such rules are actually on.
        if 'emscripten' in str(run_target.get_build_folder()):
            raise unittest.SkipTest('skipping test because emscripten')
        if platform.system() == 'Windows':
            raise unittest.SkipTest('zplayer does not print to stdout on Windows')

        cases = [
            # 2.50.1 (build 28)
            ('new2013/new2013.qst', ['qr_TURN_WHILE_CHARGING_HAMMER 1']),
            # 2.10
            ('hero_of_dreams/hero_of_dreams.qst', ['qr_OLD_210_HAMMER_POUND_REACH 1']),
        ]
        for qst, expected_lines in cases:
            with self.subTest(msg=qst):
                p = run_target.check_run(
                    'zplayer',
                    ['-headless', '-dump-qrs', str(root_dir / 'tests/replays' / qst)],
                )
                lines = p.stdout.splitlines()
                for expected in expected_lines:
                    self.assertIn(expected, lines)

    # The pictures of frames saved with a replay (-replay-save-frames) are what the editor shows
    # as how a frame looked when it was recorded, next to the screenshot of how it looks after an
    # Assert fails. They must be the same pictures for the same frames, or every comparison would
    # show differences that aren't there.
    def test_replay_frame_pictures_match_screenshots(self):
        if 'emscripten' in str(run_target.get_build_folder()):
            raise unittest.SkipTest('skipping test because emscripten')

        from PIL import Image

        def pixels(path: Path):
            with Image.open(path) as image:
                return image.size, image.convert('RGB').tobytes()

        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            replay = tmp / 'keys_v8.zplay'
            shutil.copy(root_dir / 'tests/replays/misc/keys_v8.zplay', replay)
            shutil.copy(root_dir / 'tests/replays/misc/keys.qst', tmp / 'keys.qst')

            # zplayer doesn't make its output folders.
            (tmp / 'update').mkdir()
            run_target.check_run(
                'zplayer',
                [
                    '-headless',
                    '-update',
                    replay,
                    '-replay-save-frames',
                    '-replay-exit-when-done',
                    '-replay-output-dir',
                    tmp / 'update',
                ],
            )
            frames_path = tmp / 'keys_v8.zplay.frames'
            self.assertTrue(frames_path.exists())

            num_frames = next(
                int(line.split()[2])
                for line in replay.read_text().splitlines()
                if line.startswith('M frames ')
            )
            # Replays don't take a screenshot of frame 0.
            frames = list(range(1, num_frames, 32))
            snapshot_dir = tmp / 'snapshots'
            snapshot_dir.mkdir()
            # Asserted, like the editor does to get the screenshots it shows.
            run_target.check_run(
                'zplayer',
                [
                    '-headless',
                    '-assert',
                    replay,
                    '-snapshot',
                    ' '.join(str(f) for f in frames),
                    '-replay-exit-when-done',
                    '-replay-output-dir',
                    snapshot_dir,
                ],
            )

            different = []
            for frame in frames:
                snapshot = snapshot_dir / f'keys_v8.zplay.{frame}.png'
                self.assertTrue(snapshot.exists(), f'no screenshot of frame {frame}')
                picture = tmp / f'picture.{frame}.png'
                run_target.check_run(
                    'zplayer',
                    ['-headless', '-extract-replay-frame', frames_path, str(frame), picture],
                )
                # Compared by color rather than by file: a picture is only saved when the
                # frame looks different, so its palette can differ in colors no pixel uses.
                if pixels(picture) != pixels(snapshot):
                    different.append(frame)
            self.assertEqual(different, [])

    def test_non_ascii_quest_path(self):
        # On Windows, narrow-char APIs (argv, fopen, ...) read paths in the process code
        # page, which src/windows.manifest sets to UTF-8. Without it these fail to load:
        # 'クエスト' isn't in any Western code page.
        if 'emscripten' in str(run_target.get_build_folder()):
            raise unittest.SkipTest('skipping test because emscripten')

        with tempfile.TemporaryDirectory() as tmp:
            for folder in ['Créateur', 'クエスト']:
                with self.subTest(msg=folder):
                    qst = Path(tmp) / folder / 'playground.qst'
                    qst.parent.mkdir()
                    shutil.copy(root_dir / 'tests/replays/playground/playground.qst', qst)
                    run_target.check_run('zplayer', ['-headless', '-dump-qrs', str(qst)])

    def test_base_test_runner(self):
        if 'emscripten' in str(run_target.get_build_folder()):
            raise unittest.SkipTest('skipping test because emscripten')

        run_target.check_run('base_test_runner', [])


if __name__ == '__main__':
    unittest.main()
