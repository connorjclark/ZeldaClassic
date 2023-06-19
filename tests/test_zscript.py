# Compiles every script in tests/scripts and asserts the resulting ZASM is as expected.
#
# To update the ZASM snapshots:
#
#   python tests/test_zscript.py --update
#
# To add a new script to the tests, simply add it to tests/scripts.

import argparse
import os
import sys
import subprocess
import unittest
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--update', action='store_true', default=False,
                    help='Update ZASM snapshots')
parser.add_argument('unittest_args', nargs='*')
args = parser.parse_args()

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent
test_scripts_dir = root_dir / 'tests/scripts'


class TestReplays(unittest.TestCase):
    def setUp(self):
        self.maxDiff = None

    def compile_script(self, script_path):
        build_folder = root_dir / 'build/Release'
        if 'BUILD_FOLDER' in os.environ:
            build_folder = Path(os.environ['BUILD_FOLDER']).absolute()
        exe_name = 'zscript.exe' if os.name == 'nt' else 'zscript'
        args = [
            build_folder / exe_name,
            '-input', script_path,
            '-zasm', 'out.zasm',
            '-unlinked'
        ]
        output = subprocess.run(args, cwd=build_folder, encoding='utf-8',
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if output.returncode != 0:
            raise Exception(f'got error: {output.returncode}\n{output.stdout}')
        zasm = Path(build_folder / 'out.zasm').read_text()

        # Remove metadata.
        zasm = '\n'.join([l.strip() for l in zasm.splitlines()
                         if not l.startswith('#')]).strip()

        return zasm

    def test_zscript_compiler_expected_zasm(self):
        for script_path in test_scripts_dir.rglob('*.zs'):
            with self.subTest(msg=f'compile {script_path.name}'):
                zasm = self.compile_script(script_path)
                expected_path = script_path.with_name(
                    f'{script_path.stem}_expected.txt')

                expected_zasm = None
                if expected_path.exists():
                    expected_zasm = expected_path.read_text()

                if args.update:
                    if expected_zasm != zasm:
                        print(f'updating zasm snapshot {expected_path.name}')
                        expected_path.write_text(zasm)
                else:
                    if expected_zasm == None:
                        raise Exception('f{expected_path} does not exist')
                    self.assertEqual(expected_zasm, zasm)


if __name__ == '__main__':
    sys.argv[1:] = args.unittest_args
    unittest.main()
