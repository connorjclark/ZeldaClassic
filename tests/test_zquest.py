# Tests ZQuest.
#
# To run:
#
#   python tests/test_zquest.py

import sys
import os
import json
import subprocess
import unittest
import shutil
import platform
from pathlib import Path
from common import ReplayTestResults

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent
tmp_dir = root_dir / '.tmp/test_zquest'
tmp_dir.mkdir(exist_ok=True, parents=True)

sys.path.append(str((root_dir / 'scripts').absolute()))
import run_target
from protocol.protocol_client import ZCProtocol

class TestZQuest(unittest.TestCase):
    def setUp(self):
        self.maxDiff = None

    def quick_assign(self, qst_path):
        args = [
            '-headless',
            '-quick-assign',
            qst_path,
        ]
        run_target.check_run('zquest', args)

    def run_replay(self, output_dir, args):
        args = [
            sys.executable,
            root_dir / 'tests/run_replay_tests.py',
            '--build_folder', run_target.get_build_folder(),
            '--test_results', output_dir,
            *args,
        ]
        output = subprocess.run(args, stdout=subprocess.PIPE, encoding='utf-8')
        test_results_path = output_dir / 'test_results.json'
        if not test_results_path.exists():
            print(output.stdout)
            raise Exception('could not find test_results.json')

        test_results_json = json.loads(test_results_path.read_text('utf-8'))
        results = ReplayTestResults(**test_results_json)

        run = results.runs[0][0]
        if not run.success:
            failing_str = f'failure at frame {run.failing_frame}'
            if run.unexpected_gfx_segments:
                segments_str = [
                    f'{r[0]}-{r[1]}' for r in run.unexpected_gfx_segments]
                failing_str += ': ' + ', '.join(segments_str)
            self.fail(failing_str)
        self.assertEqual(output.returncode, 0)

    # Simply open ZQuest with the new quest template.
    def test_file_new(self):
        run_target.check_run('zquest', [
            '-headless',
            '-s',
            '-q',
            'modules/classic/classic_1st.qst',
        ])

    # Resave classic_1st.qst and assert classic_1st.zplay, to make sure the loading/saving code is not introducing bugs.
    def test_save(self):
        # TODO: Bad exit code 0xFFFF under windows.
        if platform.system() == 'Windows':
            return
        # TODO: disabled b/c of recent subscreen changes, must find where regression is
        return

        qst_path = tmp_dir / 'tmp.qst'
        run_target.check_run('zquest', [
            '-headless',
            '-s',
            '-copy-qst', 'modules/classic/classic_1st.qst', qst_path,
        ])

        replay_content = (root_dir / 'tests/replays/classic_1st.zplay').read_text('utf-8')
        replay_content = replay_content.replace('modules/classic/classic_1st.qst', 'tmp.qst')
        replay_path = tmp_dir / 'tmp.zplay'
        replay_path.write_text(replay_content)

        output_dir = tmp_dir / 'output' / replay_path.name
        self.run_replay(output_dir, [replay_path])

    def test_compile_and_quick_assign(self):
        # TODO: set this via CLI
        include_paths = [
            str(root_dir / 'tests/scripts'),
            str(root_dir / 'resources/include'),
            str(root_dir / 'resources/headers'),
        ]
        (run_target.get_build_folder() / 'includepaths.txt').write_text(';'.join(include_paths))

        # Make copy of playground.qst
        qst_path = tmp_dir / 'playground.qst'
        shutil.copy(root_dir / 'tests/replays/playground.qst', qst_path)

        # Re-compile and assign slots.
        self.quick_assign(qst_path)

        # Ensure replays continue to pass.
        for original_replay_path in (root_dir / 'tests/replays').glob('playground_*.zplay'):
            with self.subTest(msg=f'{original_replay_path.name}'):
                replay_path = tmp_dir / 'tmp.zplay'
                shutil.copy(original_replay_path, replay_path)

                output_dir = tmp_dir / 'output' / original_replay_path.name
                output_dir.mkdir(exist_ok=True, parents=True)
                self.run_replay(output_dir, [replay_path])

    def test_protocol(self):
        sp = run_target.Popen('zquest', [
            # '-headless',
            '-test-skip-warnings',
            '-protocol-server',
            '-new',
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        p = ZCProtocol('ws://localhost:9002')

        r = p.wait_for_event('quest_loaded')
        self.assertEqual(r['method'], 'quest_loaded')
        self.assertEqual(r['params']['path'], 'modules/classic/default.qst')

        r = p.send_command('load_quest', {'path': 'modules/classic/classic_1st.qst'})
        self.assertEqual(r['id'], 1)
        self.assertEqual(r['result']['success'], True)
        self.assertEqual(r['result']['error_code'], 0)

        r = p.wait_for_event('quest_loaded')
        self.assertEqual(r['method'], 'quest_loaded')
        self.assertEqual(r['params']['path'], 'modules/classic/classic_1st.qst')

        r = p.send_command('get_item_names')
        self.assertEqual(r['id'], 2)
        self.assertEqual(len(r['result']['names']), 256)
        self.assertEqual(r['result']['names'][0], 'Rupee (1)')
        self.assertEqual(r['result']['names'][1], 'Rupee (5)')

        r = p.send_command('get_items')
        self.assertEqual(r['id'], 3)
        self.assertEqual(len(r['result']['items']), 256)
        self.assertEqual(r['result']['items'][0], {'id': 0, 'name': 'Rupee (1)', 'tile': 49})
        self.assertEqual(r['result']['items'][1], {'id': 1, 'name': 'Rupee (5)', 'tile': 49})
        self.assertEqual(r['result']['items'][5], {'id': 5, 'name': 'Sword 1 (Wooden)', 'tile': 41})

        p.send_command_async('quit')

        # while p.wait_for_any_event():
        #     event = p.pop_event()
        #     print(event['method'])
        #     if event['method'] == 'dialog_opened':
        #         for widget in event['params']['widgets']:
        #             print(widget)
        #     else:
        #         print(event['params'])

        if sp.wait() != 0:
            print('stdout\n', sp.stdout)
            print('stderr\n', sp.stderr)

if __name__ == '__main__':
    unittest.main()
