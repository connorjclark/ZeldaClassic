import subprocess
import os
from pathlib import Path

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent
header_path = root_dir / 'src/base/version_fallback.h'

command = 'git describe --tags --abbrev=0 --match *.*.*'
try:
    version = subprocess.check_output(command, shell=True, encoding='utf-8').strip()
except:
    # TODO: remove once first version is posted.
    version = '3.0.0'
header_path.write_text(f'#define ZC_VERSION_FALLBACK "{version}"\n')
