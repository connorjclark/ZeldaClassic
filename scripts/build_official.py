# For CI. If you want to build locally, you don't need this script.

import argparse
import os
import subprocess
import platform
from pathlib import Path

parser = argparse.ArgumentParser(description='Builds for release, including PGO (Performance Guided Optimization)')
parser.add_argument('--build-folder', default='build_official')
parser.add_argument('--release-tag', required=True)
parser.add_argument('--config', required=True)
parser.add_argument('--arch', required=True)
parser.add_argument('--repo', required=True)
parser.add_argument('--target', required=True)
parser.add_argument('--sentry', action='store_true')
parser.add_argument('--pgo', action='store_true')

args = parser.parse_args()

script_dir = Path(os.path.dirname(os.path.realpath(__file__)))
root_dir = script_dir.parent

arch = args.arch

system = platform.system()
if system == 'Windows':
    arch_label = 'x86' if args.arch == 'win32' else 'x64'
    channel = f'windows-{arch_label}'
elif system == 'Darwin':
    channel = 'mac'
elif system == 'Linux':
    channel = 'linux'

def gha_warning(message):
    if 'CI' in os.environ:
        print(f'::warning file=scripts/build_official.py,title=Build warning::{message}')
    else:
        print(f'WARNING: {message}')

def configure():
    subprocess.check_call([
        'cmake',
        '-S', '.',
        '-B', args.build_folder,
        '-G', 'Ninja Multi-Config',
        f'-DWANT_PGO={args.pgo}',
        f'-DRELEASE_TAG={args.release_tag}',
        f'-DRELEASE_CHANNEL={channel}',
        f'-DREPO={args.repo}',
        f'-DWANT_SENTRY={args.sentry}',
        f'-DWANT_MAC_DEBUG_FILES={args.config == "RelWithDebInfo"}',
        '-DCMAKE_WIN32_EXECUTABLE=1',
        '-DCMAKE_C_COMPILER_LAUNCHER=ccache',
        '-DCMAKE_CXX_COMPILER_LAUNCHER=ccache',
    ])

def build(config):
    subprocess.check_call([
        'cmake',
        '--build', args.build_folder,
        '--config', config,
        '--target', args.target,
        '-j', '4',
        '--', '-k', '0',
    ])

def run_replay_coverage(extra_args):
    try:
        subprocess.check_call([
            'python', 'tests/run_replay_tests.py',
            '--build_folder', f'{args.build_folder}/Coverage',
            '--replay',
            '--ci',
            *extra_args,
        ])
    except Exception as e:
        gha_warning(f'Error running replay for coverage: {e}')

def run_instrumented():
    run_replay_coverage([
        '--filter', 'playground',
        '--filter', 'classic_1st.zplay',
        '--filter', 'hollow_forest',
        '--frame=hollow_forest.zplay=10000',
    ])
    run_replay_coverage([
        '--no-jit',
        '--filter', 'yuurand.zplay',
        '--frame=10000',
    ])
    run_replay_coverage([
        '--filter', 'hero_of_dreams',
        '--frame=10000',
    ])

def collect_coverage():
    build('Coverage')

    for p in (root_dir / f'{args.build_folder}/Coverage').rglob('*.profraw'):
        p.unlink()

    run_instrumented()

    raw_files = (str(p) for p in ((root_dir / f'{args.build_folder}/Coverage').rglob('*.profraw')))
    (root_dir / f'{args.build_folder}/Coverage/profraw_list.txt').write_text('\n'.join(raw_files))
    subprocess.check_call('xcrun llvm-profdata merge -output=zc.profdata --input-files=profraw_list.txt'.split(' '), cwd=f'{args.build_folder}/Coverage')


configure()
if not args.pgo:
    build(args.config)
    exit(0)

collect_coverage()
build(args.config)
