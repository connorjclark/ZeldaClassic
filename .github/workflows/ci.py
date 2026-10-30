import argparse
import hashlib
import json
import logging
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import time
import zipfile

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Optional

import discord
import requests

script_dir = Path(__file__).parent.absolute()
root_dir = script_dir.parent.parent
tmp_dir = root_dir / ".tmp"


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S',
)
logger = logging.getLogger(__name__)


def command(name: str, help: Optional[str] = None):
    """Decorator to register a function as a CLI command."""

    def decorator(func: Callable):
        func._cli_command = name
        func._cli_help = help
        return func

    return decorator


def argument(*name_or_flags, **kwargs):
    """Decorator to register an argument for a CLI command."""

    def decorator(func: Callable):
        if not hasattr(func, "_cli_args"):
            func._cli_args = []
        # Store in reverse order because decorators are applied bottom-to-top
        func._cli_args.insert(0, (name_or_flags, kwargs))
        return func

    return decorator


@dataclass
class CiContext:
    """Shared state and platform-specific configuration for CI operations."""

    arch: str
    compiler: str
    config: str
    release_version: str
    repo: str
    build_folder_base: Path

    system: str = platform.system()

    @property
    def is_windows(self) -> bool:
        return self.system == 'Windows'

    @property
    def is_mac(self) -> bool:
        return self.system == 'Darwin'

    @property
    def is_linux(self) -> bool:
        return self.system == 'Linux'

    @property
    def platform_label(self) -> str:
        if self.is_windows:
            return f'windows-{self.arch_label}'
        if self.is_mac:
            # Arch-neutral on purpose: this becomes the compile-time
            # RELEASE_PLATFORM string, which both slices of a universal
            # binary share.
            return 'mac-universal' if self.arch == 'universal' else 'mac'
        if self.is_linux:
            return f'linux-{self.arch}'
        return self.system.lower()

    @property
    def arch_label(self) -> str:
        if self.is_windows:
            return {'win32': 'x86', 'arm64': 'arm64'}.get(self.arch, 'x64')
        return self.arch

    @property
    def vcpkg_triplet(self) -> str:
        return f'{self.arch_label}-windows'

    def get_binary_dir(self) -> Path:
        """Determines the correct binary folder path based on OS extraction logic."""
        if self.is_windows:
            return self.build_folder_base
        elif self.is_mac:
            binary_dir = (
                self.build_folder_base / "ZQuest Classic.app" / "Contents" / "Resources"
            )
        elif self.is_linux:
            binary_dir = self.build_folder_base / "bin"

        if binary_dir.exists():
            return binary_dir

        return self.build_folder_base


def run_cmd(cmd, env=None, cwd=None, shell=False, retries=0, retry_delay=0):
    """Helper to run a subprocess command with logging and error handling.

    A non-zero exit is retried up to `retries` more times, sleeping `retry_delay`
    seconds between attempts, before the script exits with the command's code.
    """
    if isinstance(cmd, str) and not shell:
        cmd_str = cmd
        # posix=False helps preserve Windows paths if they contain backslashes
        is_posix = platform.system() != 'Windows'
        cmd = shlex.split(cmd, posix=is_posix)
    elif isinstance(cmd, list):
        cmd = [str(c) for c in cmd]
        cmd_str = shlex.join(cmd)
    else:
        cmd_str = str(cmd)

    logger.info(f"Running: {cmd_str}")
    for attempt in range(retries + 1):
        try:
            subprocess.run(cmd, env=env, cwd=cwd, shell=shell, check=True)
            return
        except subprocess.CalledProcessError as e:
            if attempt < retries:
                logger.warning(
                    f"Command failed with exit code {e.returncode}, retrying in "
                    f"{retry_delay}s ({attempt + 1}/{retries}): {cmd_str}"
                )
                time.sleep(retry_delay)
                continue
            logger.error(f"Command failed with exit code {e.returncode}: {cmd_str}")
            sys.exit(e.returncode)
        except OSError as e:
            logger.error(f"Failed to execute command: {e}")
            sys.exit(1)


def run_python_cmd(cmd, env=None, cwd=None, shell=False):
    """Helper to run a python command with -Xutf8."""
    prefix = [sys.executable, "-Xutf8"]
    if shell:
        prefix_str = " ".join(shlex.quote(p) for p in prefix)
        if isinstance(cmd, list):
            cmd = f"{prefix_str} {shlex.join([str(c) for c in cmd])}"
        else:
            cmd = f"{prefix_str} {cmd}"
    else:
        if isinstance(cmd, str):
            is_posix = platform.system() != 'Windows'
            cmd = shlex.split(cmd, posix=is_posix)
        cmd = prefix + [str(c) for c in cmd]

    return run_cmd(cmd, env=env, cwd=cwd, shell=shell)


# -----------------------------------------------------------------------------
# build.yml
# -----------------------------------------------------------------------------


def configure_signatures(ctx: CiContext):
    """Generates the metadata.h C++ header file based on the target compiler."""
    logger.info(
        f"Configuring signatures for compiler: {ctx.compiler}, version: {ctx.release_version}"
    )

    metadata_dir = root_dir / "src/metadata"
    metadata_dir.mkdir(parents=True, exist_ok=True)
    metadata_file = metadata_dir / "metadata.h"

    lines = ['#define __TIMEZONE__ "UTC"', '']

    if ctx.compiler == 'msvc':
        lines.extend(
            [
                '#define COMPILER_V_FIRST (_MSC_VER/100)',
                '#define COMPILER_V_SECOND (_MSC_VER%100)',
                '#define COMPILER_V_THIRD (_MSC_FULL_VER%100000)',
                '#define COMPILER_V_FOURTH _MSC_BUILD',
                '#define COMPILER_NAME "MSVC"',
                '#define METADATA_IMPL_STRINGIFY(x) #x',
                '#define COMPILER_VERSION METADATA_IMPL_STRINGIFY(_MSC_FULL_VER)',
                '',
            ]
        )

        # Parse version for MSVC (.rc files)
        version_match = re.match(r"(\d+)\.(\d+)\.(\d+)", ctx.release_version)
        if version_match:
            lines.append(f'#define V_ZC_FIRST {version_match.group(1)}')
            lines.append(f'#define V_ZC_SECOND {version_match.group(2)}')
            lines.append(f'#define V_ZC_THIRD {version_match.group(3)}')
        else:
            logger.warning(
                f"Could not parse major.minor.patch from '{ctx.release_version}'"
            )
            lines.extend(
                [
                    '#define V_ZC_FIRST 0',
                    '#define V_ZC_SECOND 0',
                    '#define V_ZC_THIRD 0',
                ]
            )

        prerelease_match = re.match(r".*prerelease\.(\d+)", ctx.release_version)
        if prerelease_match:
            lines.append(f'#define V_ZC_FOURTH {prerelease_match.group(1)}')
        else:
            lines.append('#define V_ZC_FOURTH 0')

    elif ctx.compiler == 'clang':
        lines.extend(
            [
                '#define COMPILER_V_FIRST __clang_major__',
                '#define COMPILER_V_SECOND __clang_minor__',
                '#define COMPILER_V_THIRD __clang_patchlevel__',
                '#define COMPILER_V_FOURTH 0',
                '#define COMPILER_NAME "clang"',
                '#define COMPILER_VERSION __clang_version__',
            ]
        )

    elif ctx.compiler == 'gcc':
        lines.extend(
            [
                '#define COMPILER_V_FIRST __GNUC__',
                '#define COMPILER_V_SECOND __GNUC_MINOR__',
                '#define COMPILER_V_THIRD __GNUC_PATCHLEVEL__',
                '#define COMPILER_V_FOURTH 0',
                '#define COMPILER_NAME "gcc"',
                '#define COMPILER_VERSION __VERSION__',
            ]
        )

    else:
        logger.error(f"Unknown compiler: {ctx.compiler}")
        sys.exit(1)

    lines.append('')

    metadata_file.write_text('\n'.join(lines) + '\n')

    logger.info(f"Successfully generated {metadata_file}")


BISON_VERSION = "3.8.2"
BISON_SHA256 = "9bba0214ccf7f1079c5d59210045227bcf619519840ebfa80cd3849cff5a5bf2"
# ftp.gnu.org and the ftpmirror.gnu.org redirector are the same FSF host, so
# when it is down neither helps. Try independent mirrors first; the checksum
# makes any of them as trustworthy as the origin.
GNU_MIRRORS = [
    "https://mirrors.kernel.org/gnu",
    "https://mirrors.ocf.berkeley.edu/gnu",
    "https://mirror.csclub.uwaterloo.ca/gnu",
    "https://mirrors.dotsrc.org/gnu",
    "https://ftpmirror.gnu.org/gnu",
    "https://ftp.gnu.org/gnu",
]


def download_bison() -> Path:
    filename = f"bison-{BISON_VERSION}.tar.xz"
    tarball = Path(filename).absolute()
    for mirror in GNU_MIRRORS:
        url = f"{mirror}/bison/{filename}"
        logger.info(f"Downloading {url}")
        result = subprocess.run(
            ['curl', '-sSfL', '--connect-timeout', '20', '--max-time', '300']
            + ['-o', str(tarball), url]
        )
        if result.returncode != 0:
            logger.warning(f"Download failed (curl exit {result.returncode})")
            continue

        digest = hashlib.sha256(tarball.read_bytes()).hexdigest()
        if digest != BISON_SHA256:
            logger.warning(f"sha256 mismatch: {digest}")
            continue

        return tarball

    logger.error("Could not download bison from any mirror")
    sys.exit(1)


@command("install-deps", help="Install OS-level dependencies")
def install_deps(ctx: CiContext, args):
    logger.info("Starting dependency installation...")

    if ctx.is_mac:
        logger.info("macOS detected. Installing via Homebrew.")
        run_cmd("brew install ninja ccache")

        if platform.machine() == 'arm64':
            # Homebrew's bison is new enough here, but it is keg-only, so
            # expose it to later workflow steps via GITHUB_PATH.
            run_cmd("brew install bison")
            bison_prefix = subprocess.check_output(
                ['brew', '--prefix', 'bison'], text=True
            ).strip()
            github_path = os.environ.get('GITHUB_PATH')
            if github_path:
                with open(github_path, 'a') as f:
                    f.write(f"{bison_prefix}/bin\n")
        else:
            logger.info(f"Building Bison {BISON_VERSION}...")
            tarball = download_bison()
            run_cmd(['tar', '-xf', tarball])
            run_cmd("./configure --disable-nls", cwd=f"bison-{BISON_VERSION}")
            run_cmd(f"make -j{os.cpu_count() or 2}", cwd=f"bison-{BISON_VERSION}")
            run_cmd("sudo make install", cwd=f"bison-{BISON_VERSION}")

    elif ctx.is_linux:
        logger.info("Linux detected. Installing via apt-get.")
        run_cmd("sudo apt-get update")
        packages = [
            "ccache",
            "ninja-build",
            "build-essential",
            "libx11-dev",
            "freeglut3-dev",
            "mesa-common-dev",
            "libssl-dev",
            "libcurl4-openssl-dev",
            "libxcursor1",
            "libasound2-dev",
            "libgtk-3-dev",
            # For nativefiledialog-extended's xdg-desktop-portal backend.
            "libdbus-1-dev",
            "flex",
            "bison",
            "elfutils",
            "libpulse-dev",
            "libfreetype6-dev",
        ]
        # multilib is x86-only; these packages don't exist on arm64 Ubuntu.
        if platform.machine() in ('x86_64', 'AMD64'):
            packages += ["gcc-multilib", "g++-multilib"]
        packages_str = " ".join(packages)
        run_cmd(f"sudo apt-get install -y {packages_str}")

    elif ctx.is_windows:
        logger.info("Windows detected. Installing via Chocolatey and vcpkg.")
        # The community Chocolatey feed intermittently answers 503/504, which choco
        # reports as "package not found". Already-installed packages are skipped on
        # the next attempt, so retrying only re-fetches what failed.
        run_cmd(
            "choco install .github/dependencies.config -y --allow-downgrade",
            retries=3,
            retry_delay=30,
        )

        vcpkg_dir = root_dir / "vcpkg"
        vcpkg_exe = vcpkg_dir / "vcpkg.exe"
        triplet = ctx.vcpkg_triplet
        # run-vcpkg points vcpkg at its GitHub Actions cache backend, which vcpkg has
        # removed, so every build compiled these (~7 min, mostly OpenSSL). Keep built
        # packages in a folder instead; build.yml caches it.
        cache_dir = root_dir / '.vcpkg-cache'
        cache_dir.mkdir(exist_ok=True)
        env = os.environ.copy()
        env['VCPKG_BINARY_SOURCES'] = f'clear;files,{cache_dir},readwrite'
        run_cmd(
            f"{vcpkg_exe} install --triplet {triplet} curl openssl freetype", env=env
        )

    logger.info("Dependency installation complete.")


# The oldest macOS the mac packages support. Without this, binaries inherit
# the build machine's OS version as their minimum and refuse to launch on
# anything older. The website's "Supported operating systems" section should
# be kept in sync.
MAC_DEPLOYMENT_TARGET = "12.0"

OPENSSL_VERSION = "3.5.7"
OPENSSL_SHA256 = "a8c0d28a529ca480f9f36cf5792e2cd21984552a3c8e4aa11a24aa31aeac98e8"


def build_mac_openssl(arch: str) -> Path:
    """Build a static OpenSSL for the given arch, returning an install prefix
    usable as OPENSSL_ROOT_DIR.

    Used by mac universal builds. Homebrew no longer publishes x86_64
    bottles, so a from-source build is the only way to get an x86_64 OpenSSL
    on the arm64 runners. The native slice uses this too so both slices carry
    the same version - and unlike Homebrew's, this build reads CA
    certificates from /etc/ssl, which exists on every mac.
    """
    install_dir = tmp_dir / f"openssl-{arch}"
    if (install_dir / "lib/libssl.a").exists():
        return install_dir

    tarball = tmp_dir / f"openssl-{OPENSSL_VERSION}.tar.gz"
    if not tarball.exists():
        tmp_dir.mkdir(parents=True, exist_ok=True)
        url = f"https://github.com/openssl/openssl/releases/download/openssl-{OPENSSL_VERSION}/openssl-{OPENSSL_VERSION}.tar.gz"
        run_cmd(['curl', '-sfL', '-o', tarball, url])

    digest = hashlib.sha256(tarball.read_bytes()).hexdigest()
    if digest != OPENSSL_SHA256:
        logger.error(f"openssl tarball sha256 mismatch: {digest}")
        sys.exit(1)

    src_dir = tmp_dir / f"openssl-src-{arch}"
    src_dir.mkdir(parents=True, exist_ok=True)
    run_cmd(['tar', '-xf', tarball, '-C', src_dir, '--strip-components', '1'])

    run_cmd(
        [
            './Configure',
            f'darwin64-{arch}-cc',
            'no-shared',
            'no-tests',
            f'--prefix={install_dir}',
            '--openssldir=/etc/ssl',
            '--libdir=lib',
            # Match the deployment target of the binaries this links into.
            f'-mmacosx-version-min={MAC_DEPLOYMENT_TARGET}',
        ],
        cwd=src_dir,
    )
    run_cmd(['make', f'-j{os.cpu_count()}'], cwd=src_dir)
    # install_sw skips the (slow, unneeded) docs.
    run_cmd(['make', 'install_sw'], cwd=src_dir)
    return install_dir


# Thin Mach-O magic numbers (little-endian on disk), 64-bit and 32-bit.
MACHO_THIN_MAGICS = {b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe'}


def is_thin_macho(path: Path) -> bool:
    if path.is_symlink() or not path.is_file():
        return False
    try:
        with path.open('rb') as f:
            return f.read(4) in MACHO_THIN_MAGICS
    except OSError:
        return False


def strip_build_tree_rpaths(path: Path):
    """Remove LC_RPATH entries pointing into the build tree.

    The slices of a universal build carry rpaths naming their own build
    folders, and install_name_tool can only delete an rpath present in every
    slice of a fat file - so this must run on the thin binaries, before
    merging. (Otherwise fixup_bundle fails at package time when it tries to
    clean them up.) They are meaningless outside the build tree anyway; the
    packaging step adds the install rpath the app needs.
    """
    out = subprocess.check_output(['otool', '-l', str(path)], text=True)
    rpaths = set(re.findall(r'^\s+path (.+) \(offset \d+\)$', out, re.MULTILINE))
    for rpath in sorted(rpaths):
        if rpath.startswith(str(root_dir)):
            run_cmd(['install_name_tool', '-delete_rpath', rpath, path])


def merge_mac_binaries(primary_dir: Path, secondary_dir: Path):
    """lipo-merge every Mach-O in secondary_dir into its counterpart in primary_dir.

    This covers the executables and dylibs that get packaged, and also the
    debug info inside the .dSYM bundles next to them (dsymutil's DWARF
    companion files are Mach-O too), so the sentry debug info upload from
    primary_dir covers both architectures.
    """
    merged = []
    for path in sorted(primary_dir.rglob('*')):
        if not is_thin_macho(path):
            continue
        other = secondary_dir / path.relative_to(primary_dir)
        if not is_thin_macho(other):
            continue
        if not any(part.endswith('.dSYM') for part in path.parts):
            strip_build_tree_rpaths(path)
            strip_build_tree_rpaths(other)
        run_cmd(['lipo', '-create', path, other, '-output', path])
        merged.append(path)

    if not merged:
        logger.error(f"No binaries to merge found in {primary_dir}")
        sys.exit(1)

    logger.info(f"Created {len(merged)} universal binaries.")
    run_cmd(['lipo', '-info', merged[0]])


@command("build", help="Configure and build the project")
@argument("--no-cache", dest="cache", action="store_false", help="Disable ccache")
@argument("--official", action="store_true", help="Mark as an official build")
@argument("--test-runner", action="store_true", help="Include base_test_runner target")
@argument("--sentry", action="store_true", help="Build with Sentry support")
def build(ctx: CiContext, args):
    logger.info(
        f"Starting build process (Config: {ctx.config}, Compiler: {ctx.compiler})..."
    )

    env = os.environ.copy()

    if args.cache:
        env['CCACHE_BASEDIR'] = str(root_dir)
        env['CCACHE_DIR'] = str(root_dir / '.ccache')
        # One RelWithDebInfo build is ~400M of objects. Below that, the cache evicts
        # entries the same build needs.
        env['CCACHE_MAXSIZE'] = '800M'
        env['CCACHE_SLOPPINESS'] = 'time_macros'
    else:
        logger.info("Cache disabled.")
        env['CCACHE_DISABLE'] = '1'

    if ctx.compiler == 'gcc':
        logger.info("Forcing GCC compiler paths.")
        env['CC'] = 'gcc'
        env['CXX'] = 'g++'

    cmake_toolchain = None
    if ctx.is_windows:
        triplet = ctx.vcpkg_triplet
        cmake_toolchain = root_dir / 'vcpkg/scripts/buildsystems/vcpkg.cmake'

    targets = ['zplayer', 'zeditor', 'zscript', 'zlauncher']
    if ctx.is_windows:
        targets.append('zupdater')
    if args.test_runner:
        targets.append('base_test_runner')
    targets_str = " ".join(targets)

    configure_signatures(ctx)

    is_official = str(args.official).lower()
    wants_tests = str(args.test_runner).lower()
    sentry = str(args.sentry).lower()

    # A mac universal build compiles the host architecture into `build`, the
    # other architecture into `build-<arch>`, and lipo-merges the second into
    # the first. A single configure with both archs in CMAKE_OSX_ARCHITECTURES
    # is not supported - see "Universal (multi-arch) mac binaries" in the root
    # CMakeLists.txt.
    build_dirs = [("build", [])]
    if ctx.is_mac and ctx.arch == 'universal':
        native_arch = platform.machine()
        other_arch = 'x86_64' if native_arch == 'arm64' else 'arm64'
        # Each slice links its own arch's OpenSSL statically. Dynamic linking
        # would reference a Homebrew dylib the user won't have - today that
        # is papered over by fixup_bundle copying the dylib into the app, but
        # Homebrew has no x86_64 libraries on an arm64 host at all, so both
        # slices instead use a from-source static build.
        build_dirs = [
            (
                "build",
                [
                    "-DOPENSSL_USE_STATIC_LIBS=TRUE",
                    f"-DOPENSSL_ROOT_DIR={build_mac_openssl(native_arch)}",
                ],
            ),
            (
                f"build-{other_arch}",
                [
                    f"-DCMAKE_OSX_ARCHITECTURES={other_arch}",
                    "-DOPENSSL_USE_STATIC_LIBS=TRUE",
                    f"-DOPENSSL_ROOT_DIR={build_mac_openssl(other_arch)}",
                ],
            ),
        ]

    if args.cache:
        run_cmd("ccache -z", env=env)

    for build_dir, extra_config_args in build_dirs:
        logger.info(f"Running CMake configure ({build_dir})...")
        cmake_config_cmd = [
            "cmake",
            "-S",
            ".",
            "-B",
            build_dir,
            "-G",
            "Ninja Multi-Config",
            "-DCOPY_RESOURCES=OFF",
            f"-DZC_OFFICIAL={is_official}",
            f"-DZC_VERSION={ctx.release_version}",
            f"-DRELEASE_PLATFORM={ctx.platform_label}",
            "-DRELEASE_CHANNEL=3",
            f"-DREPO={ctx.repo}",
            f"-DWANT_SENTRY={sentry}",
            f"-DWANT_ZC_TESTS={wants_tests}",
        ]
        cmake_config_cmd.extend(extra_config_args)
        if ctx.is_mac:
            cmake_config_cmd.append(
                f"-DCMAKE_OSX_DEPLOYMENT_TARGET={MAC_DEPLOYMENT_TARGET}"
            )
        if ctx.is_windows:
            cmake_config_cmd.append("-DCMAKE_WIN32_EXECUTABLE=1")
        if ctx.is_linux and not args.official:
            # Enable DCHECK in CI on Linux for testing.
            cmake_config_cmd.append("-DCMAKE_CXX_FLAGS=-DDCHECK_IS_ON")
        if cmake_toolchain:
            cmake_config_cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={cmake_toolchain}")
            cmake_config_cmd.append(f"-DVCPKG_TARGET_TRIPLET={triplet}")
        if args.cache:
            cmake_config_cmd.extend(
                [
                    "-DCMAKE_C_COMPILER_LAUNCHER=ccache",
                    "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache",
                ]
            )

        run_cmd(cmake_config_cmd, env=env)

        logger.info(f"Running CMake build ({build_dir})...")
        run_cmd(
            f"cmake --build {build_dir} --config {ctx.config} --target {targets_str} -- -k 0",
            env=env,
        )

    if args.cache:
        run_cmd("ccache -s", env=env)

    if len(build_dirs) > 1:
        logger.info("Merging builds into universal binaries...")
        merge_mac_binaries(
            root_dir / "build" / ctx.config,
            root_dir / build_dirs[1][0] / ctx.config,
        )

    logger.info("Verifying source tree integrity...")
    run_cmd("git diff --cached --exit-code")
    logger.info("Build completed successfully.")


@command("package", help="Package the built artifacts")
@argument("--packages-dir")
@argument("--debug-info", action="store_true", help="Include debug info")
def package(ctx: CiContext, args):
    logger.info(f"Starting packaging process (Config: {ctx.config})...")

    if ctx.is_windows:
        package_name = f"{ctx.release_version}-windows-{ctx.arch_label}.zip"
    elif ctx.is_mac:
        if ctx.arch == 'universal':
            package_name = f"{ctx.release_version}-mac-universal.dmg"
        else:
            package_name = f"{ctx.release_version}-mac.dmg"
    elif ctx.is_linux:
        package_name = f"{ctx.release_version}-linux-{ctx.arch}.tar.gz"
    else:
        package_name = f"{ctx.release_version}-unknown.zip"

    logger.info(f"Target package name: {package_name}")

    packages_dir = Path(args.packages_dir)
    packages_dir.mkdir(exist_ok=True)

    if ctx.config == 'Coverage':
        logger.info("Packaging Coverage build...")
        build_dir = root_dir / "build"
        tmp_src_dir = build_dir / "tmp-src/parser"
        tmp_src_dir.mkdir(parents=True, exist_ok=True)

        shutil.copy(root_dir / "src/parser/lex.yy.cpp", tmp_src_dir)
        shutil.copy(root_dir / "src/parser/y.tab.cpp", tmp_src_dir)
        shutil.copy(root_dir / "src/parser/y.tab.hpp", tmp_src_dir)

        run_cmd(
            "tar czpvf build.tar.gz * --preserve-permissions", cwd=build_dir, shell=True
        )
        shutil.move(build_dir / "build.tar.gz", packages_dir / "build.tar.gz")

    build_dir = root_dir / "build"
    package_output_dir = build_dir / "package_output"
    package_output_dir.mkdir(parents=True, exist_ok=True)

    cpack_cmd = [
        "cpack",
        "--verbose",
        "-C",
        ctx.config,
        "-D",
        f"CPACK_PACKAGE_DIRECTORY={package_output_dir.absolute()}",
    ]
    if args.debug_info:
        cpack_cmd.extend(
            [
                "-D",
                # Mac gets a dSYM bundle; Linux has nothing like it, so just
                # skip stripping there.
                "CPACK_CUSTOM_INSTALL_VARIABLES=MAC_CREATE_DEBUG_SYMBOLS_BUNDLE=TRUE;ZC_KEEP_SYMBOLS=TRUE",
            ]
        )

    run_cmd(cpack_cmd, cwd=build_dir)

    # Move and rename the generated artifact.
    files = [f for f in package_output_dir.iterdir() if f.is_file()]
    if not files:
        logger.error(f"No package found in {package_output_dir}")
        sys.exit(1)

    ext = Path(package_name).suffix
    target_file = next((f for f in files if f.name.endswith(ext)), files[0])
    dest_path = packages_dir / package_name
    shutil.move(target_file, dest_path)
    logger.info(f"Moved {target_file.name} to {dest_path}")

    logger.info("Packaging complete.")


# -----------------------------------------------------------------------------
# test.yml
# -----------------------------------------------------------------------------


@command("extract-package", help="Unpack build artifact for testing")
@argument("--package", required=True)
@argument("--artifact-name")
def extract_package(ctx: CiContext, args):
    logger.info(f"Extracting package for testing (Config: {ctx.config})...")

    if ctx.config == 'Coverage':
        logger.info("Extracting Coverage tarball...")
        (root_dir / "build").mkdir(exist_ok=True)
        run_cmd(f'tar -xvzf "{Path(args.artifact_name) / "build.tar.gz"}" -C build')
        shutil.copytree(
            root_dir / "build/tmp-src", root_dir / "src", dirs_exist_ok=True
        )

    extract_dir = ctx.build_folder_base
    extract_dir.mkdir(parents=True, exist_ok=True)
    package_path = Path(args.package)

    if ctx.is_windows:
        logger.info("Windows detected. Extracting via 7z.")
        run_cmd(['7z', 'x', package_path.absolute()], cwd=extract_dir)

    elif ctx.is_mac:
        logger.info("macOS detected. Mounting dmg and copying app contents.")
        run_cmd(f"hdiutil attach -mountpoint .tmp/zc-mounted '{package_path}'")
        shutil.copytree(
            ".tmp/zc-mounted/ZQuest Classic.app",
            extract_dir / "ZQuest Classic.app",
            dirs_exist_ok=True,
        )
        if Path(".tmp/zc-mounted/ZQuest Classic.app.dSYM").exists():
            shutil.copytree(
                ".tmp/zc-mounted/ZQuest Classic.app.dSYM",
                extract_dir / "ZQuest Classic.app.dSYM",
                dirs_exist_ok=True,
            )
        else:
            logger.warning("Missing ZQuest Classic.app.dSYM")
        run_cmd("hdiutil unmount .tmp/zc-mounted")

    elif ctx.is_linux:
        logger.info("Linux detected. Extracting tarball.")
        run_cmd(f'tar -xvzf "{package_path}" -C "{extract_dir}"')

    logger.info(f"Package ready. Target binary dir is: {ctx.get_binary_dir()}")


@command("replay-tests", help="Run ZC replay tests")
# Omitted when running locally (e.g. tests/test_ci.py).
@argument("--runs-on")
@argument("--run-id", default="local")
@argument("--shard")
@argument("--extra-args", default="")
@argument("--no-jit", action="store_true")
@argument("--no-optimize-zasm", action="store_true")
def replay_tests(ctx: CiContext, args):
    logger.info("Preparing to run replay tests...")

    binary_dir = ctx.get_binary_dir()
    test_results_dir = tmp_dir / f"test_results/{args.run_id}"
    if args.no_jit:
        test_results_dir = test_results_dir.with_name(f"{test_results_dir.name}-nojit")
    if args.no_optimize_zasm:
        test_results_dir = test_results_dir.with_name(
            f"{test_results_dir.name}-nooptimizezasm"
        )
    test_results_dir.mkdir(parents=True, exist_ok=True)

    base_cmd = [
        'tests/run_replay_tests.py',
        f'--build_folder={binary_dir}',
        f'--build_type={ctx.config}',
        f'--test_results_folder={test_results_dir}',
        '--retries=1',
    ]
    if args.runs_on:
        base_cmd.append(f'--ci={args.runs_on}_{ctx.arch}')

    if args.shard:
        base_cmd.append(f"--shard={args.shard}")

    if args.no_jit:
        logger.info("Running some replays with JIT disabled.")
        base_cmd.extend(
            [
                "--filter=playground",
                "--filter=keys.zplay",
                "--filter=yuurand_riviere.zplay",
                "--filter=crucible_quest_short",
                "--no-jit",
                "--no-headless",
            ]
        )

    if args.no_optimize_zasm:
        logger.info("Running some replays with the ZASM optimizer disabled.")
        base_cmd.extend(
            [
                "--filter=playground",
                "--filter=keys.zplay",
                "--filter=yuurand_riviere.zplay",
                "--filter=crucible_quest_short",
                "--no-optimize-zasm",
            ]
        )

    if args.extra_args:
        logger.info(f"Applying extra arguments: {args.extra_args}")
        extra_args = shlex.split(args.extra_args)

        # Ignore any extra filter args if a fixed replay subset was selected.
        if args.no_jit or args.no_optimize_zasm:
            while "--filter" in extra_args:
                idx = extra_args.index("--filter")
                del extra_args[idx : idx + 2]

        base_cmd.extend(extra_args)

    run_python_cmd(base_cmd)


@command("unit-tests", help="Run Python unit test suite")
def unit_tests(ctx: CiContext, args):
    logger.info("Running Python unit test suite...")

    env = os.environ.copy()
    env['BUILD_FOLDER'] = str(ctx.get_binary_dir())
    # Tests that shell out to run_replay_tests.py need this to pick the right
    # timeouts and duration estimates (the build folder name alone doesn't
    # reveal the config in CI).
    env['BUILD_TYPE'] = ctx.config
    env['CXX'] = ctx.compiler

    if ctx.config != 'Coverage':
        logger.info("Updating auto script tests...")
        run_python_cmd(
            "tests/update_auto_script_tests.py",
            env=env,
        )

    logger.info("Discovering and running unittests...")
    run_python_cmd("-m unittest discover tests", env=env)

    logger.info("Running pytest unit tests...")
    run_python_cmd("-m pytest tests/test_replay_harness.py", env=env)


# -----------------------------------------------------------------------------
# coverage.yml
# -----------------------------------------------------------------------------


@command("coverage", help="Generate coverage report")
def coverage(ctx: CiContext, args):
    logger.info("Generating coverage report...")
    env = os.environ.copy()
    env['GCOV'] = "llvm-cov-14 gcov"
    run_cmd("bash tests/generate_coverage_report.sh", env=env)


# -----------------------------------------------------------------------------
# compare.yml
# -----------------------------------------------------------------------------


def extract_nested_archives(src_dir: Path):
    """Extracts downloaded zips, and then any inner tar files."""
    for filepath in src_dir.rglob("*.zip"):
        with zipfile.ZipFile(filepath, 'r') as zip_ref:
            # Extract to a folder with the same name as the zip (minus extension).
            extract_dir = filepath.parent / filepath.stem
            zip_ref.extractall(extract_dir)

    for tar_path in src_dir.rglob("*.tar"):
        with tarfile.open(tar_path, 'r') as tar_ref:
            tar_ref.extractall(tar_path.parent)


@command("compare-replays", help="Compare replays and notify Discord")
@argument("--run-id", required=True, help="Current GitHub Run ID")
@argument("--ref-name", default="unknown", help="Branch name for discord thread")
@argument("--actor", default="unknown", help="GitHub Actor for discord notification")
def compare_replays(ctx: CiContext, args):
    logger.info("Starting replay comparison process...")
    token = os.environ.get("GITHUB_TOKEN")
    surge_token = os.environ.get("SURGE_TOKEN")
    discord_webhook = os.environ.get("REPLAY_FAILURE_DISCORD_WEBHOOK")

    if not token:
        logger.error("GITHUB_TOKEN environment variable is required.")
        sys.exit(1)

    results_dir = root_dir / "test-results"
    results_dir.mkdir(exist_ok=True)

    # Fetch current run artifacts from test.yml
    logger.info(f"Fetching artifacts for current run: {args.run_id}")
    url = (
        f"https://api.github.com/repos/{ctx.repo}/actions/runs/{args.run_id}/artifacts"
    )
    headers = {
        'Authorization': f'Bearer {token}',
        'Accept': 'application/vnd.github.v3+json',
    }

    try:
        # The default page is 30, and the builds, replay shards, unit tests and
        # uploaded-replay shards of one run add up to more than that.
        response = requests.get(url, headers=headers, params={'per_page': 100})
        response.raise_for_status()
        data = response.json()
    except requests.RequestException as e:
        logger.error(f"GitHub API Error: {e}")
        sys.exit(1)

    artifacts = [
        a for a in data.get('artifacts', []) if a['name'].startswith('replays-')
    ]
    if not artifacts:
        logger.error("No test_results found. Exiting.")
        sys.exit(1)

    for artifact in artifacts:
        logger.info(f"Downloading {artifact['name']}...")
        dest = results_dir / f"{artifact['name']}.zip"
        try:
            with requests.get(
                artifact['archive_download_url'], headers=headers, stream=True
            ) as r:
                r.raise_for_status()
                with open(dest, 'wb') as f:
                    shutil.copyfileobj(r.raw, f)
        except requests.RequestException as e:
            logger.error(f"Failed to download artifact {artifact['name']}: {e}")
            sys.exit(1)

    extract_nested_archives(results_dir)

    # Collect baseline via run_test_workflow.py
    gh_out_file = root_dir / ".tmp/github_output.txt"
    gh_out_file.parent.mkdir(parents=True, exist_ok=True)
    gh_out_file.write_text("")

    env = os.environ.copy()
    env['GITHUB_OUTPUT'] = str(gh_out_file.absolute())

    # The baseline is the latest passing CI run of the branch the failing run
    # belongs to (main, a release branch, or a PR's base branch).
    run_python_cmd(
        f"tests/run_test_workflow.py --test_results {results_dir} --baseline_for_run {args.run_id} --repo {ctx.repo} --token {token}",
        env=env,
    )

    baseline_run_id = None
    for line in gh_out_file.read_text().splitlines():
        if "baseline_run_id=" in line:
            baseline_run_id = line.strip().split("=", 1)[1]

    if not baseline_run_id:
        logger.error("Failed to capture baseline_run_id")
        sys.exit(1)

    # Generate Report via compare_replays.py
    run_python_cmd(
        f"tests/compare_replays.py --workflow_run {baseline_run_id} --local {results_dir} --repo {ctx.repo} --token {token}"
    )

    # Upload to surge.sh
    report_dir = root_dir / "tests/compare-report"
    domain = f"zc-replay-compare-{args.run_id}.surge.sh"

    if surge_token:
        logger.info("Deploying report to Surge...")
        env['SURGE_TOKEN'] = surge_token
        run_cmd(f"npx surge@0.23.1 {report_dir} {domain}", env=env)
        report_url = f"https://{domain}"
    else:
        report_url = "No deployment (missing SURGE_TOKEN)"

    # Notify via Discord
    if discord_webhook:
        try:
            webhook = discord.SyncWebhook.from_url(discord_webhook)
            webhook.send(
                f"Report: {report_url}\nCI: https://github.com/{ctx.repo}/actions/runs/{args.run_id}\nActor: {args.actor}",
                thread_name=f"Replay tests failed for {args.ref_name}",
                wait=True,
            )
        except ImportError:
            logger.error("The 'discord' python package is missing.")


# -----------------------------------------------------------------------------
# CLI Entry Point
# -----------------------------------------------------------------------------


# The uploaded-replay shards of a CI run (see test.yml). A run can serve as the
# compare baseline for them only if they passed there: their baselines are
# refreshed separately from the test suite's, so the run's overall conclusion
# says nothing about them.
def _find_baseline_run_for_replay_uploads(gh, repo_str, branch, workflow_file, artifact_name):
    repo = gh.get_repo(repo_str)
    workflow = repo.get_workflow(workflow_file)
    for i, run in enumerate(workflow.get_runs(branch=branch, status='completed')):
        if i >= 40:
            break

        uploads_jobs = [job for job in _get_run_jobs(run) if 'Replay uploads' in job.name]
        if not uploads_jobs or any(job.conclusion != 'success' for job in uploads_jobs):
            continue

        artifact = next(
            (a for a in run.get_artifacts() if a.name == artifact_name and not a.expired),
            None,
        )
        if artifact:
            return run, artifact

    return None, None


@command(
    "compare-replay-uploads",
    help="Build a compare report for the failing uploaded replays of this job, against a passing build of the base branch",
)
@argument("--runs-on", required=True)
@argument("--run-id", required=True, help="Current GitHub Run ID")
@argument("--shard", default="", help="Only names the published report (ex: 2/4)")
@argument("--test-results", default=str(tmp_dir / "replay_uploads_test_results"))
@argument(
    "--baseline-build-folder",
    help="Compare against this local build instead of a CI build artifact",
)
@argument(
    "--baseline-branch",
    help="Take the baseline build from this branch's CI runs (default: the failing run's base branch)",
)
@argument(
    "--baseline-workflow",
    default="ci.yml",
    help="The workflow whose runs supply the baseline build",
)
def compare_replay_uploads(ctx: CiContext, args):
    """Unlike the test suite's compare workflow, which has to dispatch a
    baseline run on the baseline commit, this runs in the failing job itself:
    the uploaded replays and their quests are already on disk, and the baseline
    build is the build artifact of the latest run of the base branch in which
    the uploaded replays passed. The failing replays are re-run with that build
    over the frames that differed, and the two results become one report.
    """
    import dataclasses

    sys.path.append(str(root_dir / 'tests'))
    sys.path.append(str(root_dir / 'scripts'))
    from github import Github
    from github_helpers import _download_artifact
    from run_test_workflow import (
        find_baseline_branch_for_run,
        get_args_for_collect_baseline_from_test_results,
    )

    results_dir = Path(args.test_results).absolute()
    test_results_path = results_dir / 'test_results.json'
    if not test_results_path.exists():
        logger.error(f"No test results at {test_results_path}")
        sys.exit(1)

    # What this shard found, for the pull request comment (report-replay-uploads
    # gathers one of these from every shard). Written now, so a failure of this
    # step still leaves the list of failing replays, and again with the report.
    shard_parts = args.shard.split('/') if '/' in args.shard else []
    shard_slug = '-'.join(shard_parts) or 'x'
    summary_dir = tmp_dir / 'replay_uploads_summary'
    summary_dir.mkdir(parents=True, exist_ok=True)
    summary = {
        'shard': args.shard,
        'run_id': args.run_id,
        'failing': _summarize_failing_uploaded_replays(test_results_path),
        'report_url': None,
        'report_artifact': (
            f'replay-uploads-compare-report-{shard_parts[0]}-of-{shard_parts[1]}'
            if shard_parts
            else None
        ),
        'baseline': None,
        'note': None,
    }

    def write_summary():
        (summary_dir / f'shard-{shard_slug}.json').write_text(
            json.dumps(summary, indent=2), 'utf-8'
        )

    write_summary()

    # The failing replays as paths, plus the frames to snapshot around each
    # failure. Raises when every failure was a crash or a stop without a frame:
    # nothing to compare then, which is not an error of this step.
    try:
        replay_args = get_args_for_collect_baseline_from_test_results([test_results_path])
    except Exception as e:
        logger.warning(f"Nothing to compare: {e}")
        summary['note'] = 'Nothing to compare: every failure was a crash or a stop without a frame.'
        write_summary()
        return

    baseline_dir = tmp_dir / 'replay_uploads_baseline'
    if baseline_dir.exists():
        shutil.rmtree(baseline_dir)
    baseline_dir.mkdir(parents=True)

    # The nightly Asan run has no Asan build to compare against, and a
    # RelWithDebInfo build draws the same frames.
    baseline_config = 'RelWithDebInfo' if ctx.config == 'Asan' else ctx.config

    if args.baseline_build_folder:
        baseline_bin = Path(args.baseline_build_folder).absolute()
        baseline_run_id = 'local'
        baseline_label = 'local baseline'
    else:
        token = os.environ.get('GITHUB_TOKEN')
        if not token:
            logger.error("GITHUB_TOKEN environment variable is required.")
            sys.exit(1)
        gh = Github(token)

        branch = args.baseline_branch or find_baseline_branch_for_run(
            gh, ctx.repo, int(args.run_id)
        )
        artifact_name = f'build-{args.runs_on}-{ctx.arch}-{ctx.compiler}-{baseline_config}'
        logger.info(f"Looking for a baseline build on {branch}: {artifact_name}")
        run, artifact = _find_baseline_run_for_replay_uploads(
            gh, ctx.repo, branch, args.baseline_workflow, artifact_name
        )
        if not run:
            logger.warning(
                f"No recent {args.baseline_workflow} run on {branch} passed the uploaded replays and still has {artifact_name}, so there is nothing to compare against."
            )
            summary['note'] = (
                f'No compare report: no recent run on {branch} passed the uploaded replays and still has its build.'
            )
            write_summary()
            return

        baseline_run_id = run.id
        baseline_label = f'{branch} {run.head_sha[:10]} (baseline)'
        logger.info(f"Baseline: run {run.id}, {run.head_sha}")

        artifact_dir = baseline_dir / 'artifact'
        logger.info(f"Downloading {artifact.name}...")
        _download_artifact(gh, ctx.repo, artifact, artifact_dir)
        package = next(p for p in artifact_dir.iterdir() if p.name != 'build.tar.gz')

        baseline_ctx = dataclasses.replace(
            ctx, config=baseline_config, build_folder_base=baseline_dir / 'zc-extracted'
        )
        extract_package(
            baseline_ctx,
            argparse.Namespace(package=str(package), artifact_name=str(artifact_dir)),
        )
        baseline_bin = baseline_ctx.get_binary_dir()

    baseline_results = baseline_dir / 'test_results'
    cmd = [
        sys.executable,
        '-Xutf8',
        root_dir / 'tests/run_replay_tests.py',
        '--assert',
        f'--build_folder={baseline_bin}',
        f'--build_type={baseline_config}',
        f'--test_results_folder={baseline_results}',
        f'--root_replays_folder={tmp_dir / "replay_uploads"}',
        f'--ci={args.runs_on}_{ctx.arch}',
        '--no_report_on_failure',
        *replay_args,
    ]
    logger.info(f"Running: {shlex.join(str(c) for c in cmd)}")
    # A replay that also fails on the baseline build still gets its snapshots,
    # and the report shows it, so the exit code is not checked.
    subprocess.run(cmd)
    if not (baseline_results / 'test_results.json').exists():
        logger.error("The baseline run produced no test results")
        sys.exit(1)

    # Both runs happened on this runner, so tell them apart in the report.
    def relabel(path: Path, run_id, git_ref: str):
        test_results = json.loads(path.read_text('utf-8'))
        test_results['ci'] = True
        test_results['workflow_run_id'] = run_id
        test_results['git_ref'] = git_ref
        path.write_text(json.dumps(test_results, indent=2), 'utf-8')

    relabel(baseline_results / 'test_results.json', baseline_run_id, baseline_label)
    current_ref = os.environ.get('GITHUB_REF_NAME') or os.environ.get('GITHUB_REF') or 'current'
    relabel(test_results_path, args.run_id, f'{current_ref} (current)')

    report_dir = tmp_dir / 'replay_uploads_compare_report'
    if report_dir.exists():
        shutil.rmtree(report_dir)
    report_dir.mkdir(parents=True)
    run_python_cmd(
        [
            'tests/compare_replays.py',
            '--local',
            baseline_results,
            '--local',
            results_dir,
            '--dest',
            report_dir,
        ]
    )

    report_url = None
    surge_token = os.environ.get('SURGE_TOKEN')
    if surge_token:
        shard = re.sub(r'[^0-9]+', '-', args.shard).strip('-')
        domain = f"zc-replay-uploads-compare-{args.run_id}-{shard or 'x'}.surge.sh"
        env = os.environ.copy()
        env['SURGE_TOKEN'] = surge_token
        try:
            run_cmd(f"npx surge@0.23.1 {report_dir} {domain}", env=env)
            report_url = f"https://{domain}"
        except SystemExit:
            # The report is still uploaded as an artifact by the workflow.
            logger.error("Deploying the report to surge failed")

    summary['report_url'] = report_url
    summary['baseline'] = baseline_label
    write_summary()

    lines = [
        '## Uploaded replays compare report',
        '',
        f'Baseline: {baseline_label}',
        '',
        report_url or 'The report is attached to this run as an artifact.',
    ]
    logger.info('\n'.join(lines))
    summary_path = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary_path:
        with open(summary_path, 'a', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')


# The known good json's key for a local copy of an uploaded replay
# (.tmp/replay_uploads/<qst hash>/<id>-updated-main.zplay -> <qst hash>/<id>.zplay).
def _uploaded_replay_key(path: Path) -> str:
    parts = path.parts
    if 'replay_uploads' in parts:
        parts = parts[parts.index('replay_uploads') + 1 :]
    key = '/'.join(parts)
    return re.sub(r'-updated-[^./]+\.zplay$', '.zplay', key)


# One entry per replay that failed in the last run of a test results file: its
# key in the known good json, its quest's title (when the replay is on disk),
# and how it failed.
def _summarize_failing_uploaded_replays(test_results_path: Path) -> list[dict]:
    sys.path.append(str(root_dir / 'tests/lib'))
    import replay_helpers

    test_results = json.loads(test_results_path.read_text('utf-8'))
    failing = []
    for run in test_results['runs'][-1]:
        if run['success']:
            continue
        path = Path(run['path'])
        title = ''
        if path.exists():
            meta = replay_helpers.read_replay_meta(path)
            title = meta.get('qst_title') or meta.get('qst', '')
        frame = run.get('failing_frame')
        if run.get('rng_desync'):
            how = f'rng desync at frame {frame}'
        elif frame is not None:
            how = f'differs at frame {frame}'
        elif run.get('stopped'):
            how = 'stopped'
        else:
            how = f'crashed (exit code {run.get("exit_code")})'
        failing.append(
            {
                'key': _uploaded_replay_key(path),
                'title': title,
                'frame': frame,
                'how': how,
            }
        )
    return failing


def _github_from_env():
    from github import Github

    token = os.environ.get('GITHUB_TOKEN')
    if not token:
        logger.error("GITHUB_TOKEN environment variable is required.")
        sys.exit(1)
    return Github(token)


def _get_run_jobs(run):
    from github import PaginatedList
    from workflow_job import WorkflowJob

    # TODO: remove when https://github.com/PyGithub/PyGithub/pull/1951 is released.
    return list(
        PaginatedList.PaginatedList(
            WorkflowJob, run._requester, run.jobs_url, dict(), list_item='jobs'
        )
    )


# Each marker identifies one comment per pull request, which later runs edit
# rather than piling up a new comment per push.
REPLAY_UPLOADS_REPORT_MARKER = '<!-- zc-replay-uploads-report -->'
REPLAY_UPLOADS_REFRESH_MARKER = '<!-- zc-replay-uploads-refresh -->'
REPLAY_UPLOADS_REFRESH_LABEL = 'refresh-uploaded-replays'


def _upsert_pr_comment(pr, marker: str, body: str, create: bool = True):
    body = f'{marker}\n{body}'
    for comment in pr.get_issue_comments():
        if comment.body and comment.body.startswith(marker):
            comment.edit(body)
            return comment
    if create:
        return pr.create_issue_comment(body)
    return None


def _replay_cell(entry: dict) -> str:
    qst_hash, _, name = entry['key'].partition('/')
    return f'`{qst_hash[:8]}…/{name[:8]}…`'


@command(
    "report-replay-uploads",
    help="Comment on the pull request with the uploaded replays that failed and where their compare reports are",
)
@argument("--run-id", required=True, help="Current GitHub Run ID")
@argument("--pr", required=True, type=int, help="The pull request number")
@argument(
    "--result",
    required=True,
    help="The result of the Replay uploads jobs (success, failure, cancelled, ...)",
)
@argument("--summaries", default=str(tmp_dir / "replay_uploads_summary"))
def report_replay_uploads(ctx: CiContext, args):
    """Runs after every Replay uploads shard of a pull request's CI run. On a
    failure it gathers the shards' summaries (see compare-replay-uploads) into
    one comment: which replays failed, how, and the compare report of each
    shard. Later runs edit that comment, so a PR has at most one.
    """
    gh = _github_from_env()
    repo = gh.get_repo(ctx.repo)
    pr = repo.get_pull(args.pr)
    run = repo.get_workflow_run(int(args.run_id))
    sha = run.head_sha[:10]

    if args.result == 'success':
        # Only an earlier failure's comment is worth updating.
        _upsert_pr_comment(
            pr,
            REPLAY_UPLOADS_REPORT_MARKER,
            f'## Uploaded replays: all pass on {sha}\n\n'
            f'An earlier commit failed some of them (see this comment\'s edit history).',
            create=False,
        )
        return
    if args.result != 'failure':
        logger.info(f"Replay uploads jobs ended with {args.result}, nothing to report")
        return

    summaries = [
        json.loads(p.read_text('utf-8')) for p in sorted(Path(args.summaries).glob('*.json'))
    ]
    failing = [(entry, s) for s in summaries for entry in s['failing']]

    lines = []
    if not failing:
        lines += [
            f'## Uploaded replays: the jobs failed on {sha}',
            '',
            f'They failed before running any replay, so there is nothing to compare. See [the run]({run.html_url}).',
        ]
    else:
        lines += [
            f'## Uploaded replays: {len(failing)} failed on {sha}',
            '',
            '| Replay | Quest | Failure | Compare report |',
            '|---|---|---|---|',
        ]
        for entry, s in sorted(failing, key=lambda f: f[0]['key']):
            if s['report_url']:
                report = f"[shard {s['shard']}]({s['report_url']})"
            elif s['report_artifact'] and not s['note']:
                report = f"artifact `{s['report_artifact']}`"
            else:
                report = 'none'
            lines.append(
                f"| {_replay_cell(entry)} | {entry['title']} | {entry['how']} | {report} |"
            )
        baselines = sorted({s['baseline'] for s in summaries if s['baseline']})
        notes = sorted({s['note'] for s in summaries if s['note']})
        lines += [
            '',
            '<details><summary>Replay keys</summary>',
            '',
        ]
        lines += [f"- `{entry['key']}`" for entry, _ in sorted(failing, key=lambda f: f[0]['key'])]
        lines += ['', '</details>', '']
        if baselines:
            lines.append(
                f"Baseline: {', '.join(baselines)}. The report shows the first differing frame of each replay, and the frames around it."
            )
        lines += notes
        lines += [
            '',
            f'If this is an intended change, add the `{REPLAY_UPLOADS_REFRESH_LABEL}` label to this pull request: '
            'the baselines of these replays are re-recorded with the CI build of this commit, '
            'the new pins are committed to this branch, and CI runs again.',
        ]

    _upsert_pr_comment(pr, REPLAY_UPLOADS_REPORT_MARKER, '\n'.join(lines))
    logger.info('\n'.join(lines))


@command(
    "refresh-replay-uploads",
    help="Re-record the known good uploaded replays' baselines and commit the new pins; for a pull request, only its failing replays, with its CI build",
)
@argument(
    "--pr",
    type=int,
    help="Refresh the replays that failed in this pull request's latest CI run, with that run's build, and comment on it",
)
@argument(
    "--runs-on",
    default="ubuntu-22.04",
    help="With --pr: the CI platform whose build and results to use",
)
@argument("--no-commit", action="store_true", help="Leave the refreshed json uncommitted")
def refresh_replay_uploads(ctx: CiContext, args):
    """Two ways in. From a pull request (the refresh-uploaded-replays label):
    takes the failing replays and the build from the PR's latest CI run, so
    nothing is rebuilt, re-records just those, commits the json to the PR
    branch, dispatches CI on it (a push by the Actions token starts no run on
    its own) and leaves a comment. From a workflow_dispatch: re-records every
    known good replay with the build at --build-folder, and commits to the
    branch the workflow ran on.
    """
    sys.path.append(str(root_dir / 'scripts'))
    sys.path.append(str(root_dir / 'tests'))
    from github_helpers import _download_artifact

    known_good_path = root_dir / 'scripts/replay_uploads_known_good_replays.json'
    cmd = [
        sys.executable,
        '-Xutf8',
        root_dir / 'scripts/replay_uploads.py',
        '--refresh_known_good',
    ]

    pr = None
    compare_links = []
    source = 'this branch'

    def finish(message: str, ok: bool):
        if pr:
            logger.info(message)
            _upsert_pr_comment(
                pr, REPLAY_UPLOADS_REFRESH_MARKER, f'## Uploaded replays refresh\n\n{message}'
            )
            try:
                pr.remove_from_labels(REPLAY_UPLOADS_REFRESH_LABEL)
            except Exception as e:
                logger.warning(f"Could not remove the label: {e}")
        elif ok:
            logger.info(message)
        else:
            logger.error(message)
        sys.exit(0 if ok else 1)

    if args.pr:
        gh = _github_from_env()
        repo = gh.get_repo(ctx.repo)
        pr = repo.get_pull(args.pr)
        head_sha = pr.head.sha
        if not pr.head.repo or pr.head.repo.full_name != ctx.repo:
            finish(
                'The baselines can only be refreshed from a branch of this repository, not a fork: '
                'the workflow has to push the new pins to the branch.',
                ok=False,
            )

        # The PR's latest CI run must be for its current head, and done.
        run = next(
            (
                r
                for r in repo.get_workflow('ci.yml').get_runs(
                    branch=pr.head.ref, event='pull_request'
                )
                if r.head_sha == head_sha
            ),
            None,
        )
        if not run:
            finish(f'No CI run for {head_sha[:10]} yet. Add the label again once it has finished.', ok=False)
        if run.status != 'completed':
            finish(
                f'CI is still running for {head_sha[:10]} ([run]({run.html_url})). Add the label again once it has finished.',
                ok=False,
            )
        uploads_jobs = [j for j in _get_run_jobs(run) if 'Replay uploads' in j.name]
        if not uploads_jobs:
            finish(f'[The CI run]({run.html_url}) for {head_sha[:10]} did not run the uploaded replays.', ok=False)
        if all(j.conclusion == 'success' for j in uploads_jobs):
            finish(f'Every uploaded replay passed in [the CI run]({run.html_url}) for {head_sha[:10]}, so there is nothing to refresh.', ok=True)

        work_dir = tmp_dir / 'replay_uploads_refresh'
        if work_dir.exists():
            shutil.rmtree(work_dir)
        work_dir.mkdir(parents=True)
        artifacts = [a for a in run.get_artifacts() if not a.expired]

        # The failing replays, from the test results the failing shards uploaded.
        keys = []
        results_prefix = f'replay-uploads-{args.runs_on}-{ctx.arch}-{ctx.compiler}-'
        for artifact in artifacts:
            if not artifact.name.startswith(results_prefix):
                continue
            dest = work_dir / artifact.name
            _download_artifact(gh, ctx.repo, artifact, dest)
            for tar_path in dest.glob('*.tar'):
                with tarfile.open(tar_path) as tar:
                    tar.extractall(dest / 'results')
            results_path = dest / 'results' / 'test_results.json'
            if results_path.exists():
                keys += [f['key'] for f in _summarize_failing_uploaded_replays(results_path)]
        keys = sorted(set(keys))
        if not keys:
            finish(
                f'The failing Replay uploads jobs of [the CI run]({run.html_url}) left no test results, '
                'so they failed before running any replay. See their logs.',
                ok=False,
            )

        # The compare reports those shards made, for the comment.
        for artifact in artifacts:
            if artifact.name.startswith('replay-uploads-summary-'):
                dest = work_dir / artifact.name
                _download_artifact(gh, ctx.repo, artifact, dest)
                for p in dest.glob('*.json'):
                    s = json.loads(p.read_text('utf-8'))
                    if s.get('report_url'):
                        compare_links.append(f"[shard {s['shard']}]({s['report_url']})")

        # The build of that run: what failed is what records the new baselines.
        build_artifact_name = f'build-{args.runs_on}-{ctx.arch}-{ctx.compiler}-{ctx.config}'
        artifact = next((a for a in artifacts if a.name == build_artifact_name), None)
        if not artifact:
            finish(
                f'[The CI run]({run.html_url}) no longer has its build ({build_artifact_name}). Push a new commit and add the label again.',
                ok=False,
            )
        artifact_dir = work_dir / 'build'
        logger.info(f"Downloading {artifact.name}...")
        _download_artifact(gh, ctx.repo, artifact, artifact_dir)
        package = next(p for p in artifact_dir.iterdir() if p.name != 'build.tar.gz')
        extract_package(
            ctx, argparse.Namespace(package=str(package), artifact_name=str(artifact_dir))
        )
        cmd += ['--build_folder', ctx.get_binary_dir()]
        for key in keys:
            cmd += ['--only', key]
        source = f'the CI build of {head_sha[:10]} ([run]({run.html_url}))'
        branch = pr.head.ref
    else:
        cmd += ['--build_folder', ctx.get_binary_dir()]
        branch = os.environ.get('GITHUB_REF_NAME')

    before = json.loads(known_good_path.read_text('utf-8'))
    logger.info(f"Running: {shlex.join(str(c) for c in cmd)}")
    if subprocess.run(cmd).returncode != 0:
        finish(
            f'Re-recording with {source} failed: some replay no longer plays through, which is a bug to look into, not an intended change. See the job log.',
            ok=False,
        )
    after = json.loads(known_good_path.read_text('utf-8'))
    moved = sorted(k for k in after if after[k] != before.get(k))
    if not moved:
        finish(
            f'Re-recording with {source} gave the pinned baselines back unchanged, so there is nothing to commit. '
            'The failure is not in what the replays record; see the job logs.',
            ok=True,
        )
    if args.no_commit:
        logger.info(f"{len(moved)} baselines refreshed, left uncommitted")
        return
    if not branch:
        logger.error("No branch to commit to (GITHUB_REF_NAME is not set)")
        sys.exit(1)

    # From the json's perspective these are the pins that moved; the message
    # lists them by quest so the commit reads without the hashes.
    message_lines = [
        f'test!: refresh {len(moved)} uploaded replay baseline{"s" if len(moved) != 1 else ""}',
        '',
    ]
    if pr:
        message_lines.append(
            f'Re-recorded with the CI build of {head_sha} (run {run.id}) for #{pr.number}.'
        )
    else:
        message_lines.append('Re-recorded with this branch\'s build.')
    message_lines.append('')
    for key in moved:
        message_lines.append(key)
    message_path = tmp_dir / 'replay_uploads_refresh_commit.txt'
    message_path.write_text('\n'.join(message_lines) + '\n', 'utf-8')

    run_cmd(['git', 'config', 'user.name', 'github-actions[bot]'])
    run_cmd(
        [
            'git',
            'config',
            'user.email',
            '41898282+github-actions[bot]@users.noreply.github.com',
        ]
    )
    run_cmd(['git', 'add', str(known_good_path)])
    run_cmd(['git', 'commit', '-F', str(message_path)])
    run_cmd(['git', 'push', 'origin', f'HEAD:refs/heads/{branch}'])
    new_sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()

    if pr:
        # A push made with the Actions token starts no workflow, so ask for one.
        repo.get_workflow('ci.yml').create_dispatch(branch)
        lines = [
            f'Refreshed {len(moved)} baseline{"s" if len(moved) != 1 else ""} with {source}, committed as {new_sha[:10]}, and started CI on it.',
            '',
        ]
        if compare_links:
            lines.append(
                f"What changed, old baseline against this commit: {', '.join(compare_links)}."
            )
            lines.append('')
        lines += [f'- `{key}`' for key in moved]
        finish('\n'.join(lines), ok=True)
    logger.info(f"Refreshed {len(moved)} baselines, committed as {new_sha[:10]} on {branch}")


def main():
    parser = argparse.ArgumentParser(description="ZQuest Classic CI Script")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Global/Shared arguments for all subparsers
    parent = argparse.ArgumentParser(add_help=False)
    parent.add_argument(
        "--arch",
        default="x64",
        help="Architecture (x64, win32, arm64, intel, aarch64, universal)",
    )
    parent.add_argument("--compiler", default="gcc", help="Compiler (gcc, clang, msvc)")
    parent.add_argument(
        "--config", default="Release", help="Release, RelWithDebInfo, etc."
    )
    parent.add_argument("--release-version", help="Version string")
    parent.add_argument("--repo", required=True, help="Repo name")
    parent.add_argument("--build-folder", help="Target extraction directory")

    # Collect all functions decorated with @command
    commands = [
        obj
        for obj in globals().values()
        if callable(obj) and hasattr(obj, "_cli_command")
    ]

    # Register each command and its specific arguments
    for func in commands:
        sub = subparsers.add_parser(
            func._cli_command, help=func._cli_help, parents=[parent]
        )
        for name_or_flags, kwargs in getattr(func, "_cli_args", []):
            sub.add_argument(*name_or_flags, **kwargs)

    args = parser.parse_args()

    # Initialize the high-level context
    root = Path(__file__).parent.parent.parent.absolute()
    ctx = CiContext(
        arch=args.arch,
        compiler=args.compiler,
        config=args.config,
        release_version=args.release_version or "3.0.0",
        repo=args.repo,
        build_folder_base=Path(args.build_folder or "zc-extracted").absolute(),
    )

    # Dispatch to the appropriate function
    cmd_map = {func._cli_command: func for func in commands}
    cmd_map[args.command](ctx, args)


if __name__ == "__main__":
    main()
