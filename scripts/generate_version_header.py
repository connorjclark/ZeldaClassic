import argparse
import subprocess
import os
from pathlib import Path

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent
header_path = root_dir / 'src/base/version_header.h'

parser = argparse.ArgumentParser()
parser.add_argument('--version', nargs='?', default='')
args = parser.parse_args()

if args.version:
    version = args.version
else:
    command = 'git describe --tags --abbrev=0 --match *.*.*'
    try:
        version = subprocess.check_output(command, shell=True, encoding='utf-8').strip()
    except:
        # TODO: can remove after first version is posted.
        version = '3.0.0'
    if '+' in version:
        version += f'.local'
    else:
        version += f'+local'

code = '\n'.join([
    '// Generated by scripts/generate_version_header.py',
    '',
    f'#define ZC_VERSION "{version}"',
    '',
])
header_path.write_text(code)
