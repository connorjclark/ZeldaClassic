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
parser.add_argument('--target', default='zplayer zeditor zscript zlauncher zupdater')
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

llvm = system != 'Windows'

def section(title):
    n = 40
    print()
    print('=' * n)
    print('+' + title.center(n - 2, ' ') + '+')
    print('=' * n)
    print()

def gha_warning(message):
    if 'CI' in os.environ:
        print(f'::warning file=scripts/build_official.py,title=Build warning::{message}')
    else:
        print(f'WARNING: {message}')

def configure():
    section('Configuring build')
    cmake_args = [
        'cmake',
        '-S', '.',
        '-B', args.build_folder,
        f'-DWANT_PGO={args.pgo}',
        f'-DRELEASE_TAG={args.release_tag}',
        f'-DRELEASE_CHANNEL={channel}',
        f'-DREPO={args.repo}',
        f'-DWANT_SENTRY={args.sentry}',
        f'-DWANT_MAC_DEBUG_FILES={args.config == "RelWithDebInfo"}',
        '-DCMAKE_WIN32_EXECUTABLE=1',
        '-DCMAKE_C_COMPILER_LAUNCHER=ccache',
        '-DCMAKE_CXX_COMPILER_LAUNCHER=ccache',
    ]
    # if system != 'Windows':
    cmake_args.append('-G')
    cmake_args.append('Ninja Multi-Config')
    subprocess.check_call(cmake_args)

def build(config):
    section(f'Building for {config}')
    subprocess.check_call([
        'cmake',
        '--build', args.build_folder,
        '--config', config,
        '--target', *args.target.split(' '),
        '-j', '4',
        # '--', '-k', '0',
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

def run_python_test(path):
    print(f'running python test: {path}')
    try:
        subprocess.check_call(['python', path], env={
            **os.environ,
            'BUILD_FOLDER': f'{args.build_folder}/Coverage',
            'ZC_PGO': '1',
            # (in case running locally) Disables password checking.
            'CI': '1',
        })
    except Exception as e:
        gha_warning(f'Error running {path.name} for coverage: {e}')

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
    run_python_test(root_dir / 'tests/test_zplayer.py')
    run_python_test(root_dir / 'tests/test_zeditor.py')
    run_python_test(root_dir / 'tests/test_zscript.py')
    run_python_test(root_dir / 'tests/test_updater.py')

def collect_coverage():
    build('Coverage')
    section('Collecting coverage')

    compiler_dir = Path((Path(args.build_folder) / 'compiler.txt').read_text()).parent
    if llvm:
        for p in (root_dir / f'{args.build_folder}/Coverage').rglob('*.profraw'):
            p.unlink()
    else:
        for p in (root_dir / f'{args.build_folder}/Coverage').rglob('*.pgc'):
            p.unlink()
        for p in (root_dir / f'{args.build_folder}/Coverage').rglob('*.pgd'):
            subprocess.check_call([compiler_dir / 'pgomgr.exe', '/clear', p])

    run_instrumented()

    if llvm:
        raw_files = (str(p) for p in ((root_dir / f'{args.build_folder}/Coverage').rglob('*.profraw')))
        (root_dir / f'{args.build_folder}/Coverage/profraw_list.txt').write_text('\n'.join(raw_files))

        cmd = 'llvm-profdata merge -output=zc.profdata --input-files=profraw_list.txt'
        if 'CI' not in os.environ:
            cmd = 'xcrun ' + cmd
        subprocess.check_call(cmd.split(' '), cwd=f'{args.build_folder}/Coverage')

        section('Top functions')
        cmd = 'llvm-profdata show --topn=100 zc.profdata'
        if 'CI' not in os.environ:
            cmd = 'xcrun ' + cmd
        print(subprocess.check_output(cmd.split(' '), cwd=f'{args.build_folder}/Coverage', encoding='utf8'))
    else:
        Path(f'{args.build_folder}/{args.config}').mkdir(exist_ok=True)
        for p in (root_dir / f'{args.build_folder}/Coverage').rglob('*.pgc'):
            dest_path = Path(f'{args.build_folder}/{args.config}/{p.name}')
            dest_path.unlink(missing_ok=True)
            p.rename(dest_path)
        for p in (root_dir / f'{args.build_folder}/Coverage').rglob('*.pgd'):
            dest_path = Path(f'{args.build_folder}/{args.config}/{p.name}')
            dest_path.unlink(missing_ok=True)
            p.rename(dest_path)
        
        section('Top functions')
        for p in (root_dir / f'{args.build_folder}/{args.config}').rglob('*.pgd'):
            subprocess.check_call([compiler_dir / 'pgomgr.exe', '/merge', p.name], cwd=f'{args.build_folder}/{args.config}')
            output = subprocess.check_output([compiler_dir / 'pgomgr.exe', '/summary', p], encoding='utf8')
            print(p.name)
            print('\n'.join(output.splitlines()[:101]))


configure()
if not args.pgo:
    build(args.config)
    exit(0)

collect_coverage()
build(args.config)
