#!/bin/bash
# Runs one zplayer fuzz target. See docs/fuzzing.md.
#
#   bash scripts/fuzzing/run_fuzzer.sh <target> [jobs] [extra libFuzzer flags...]
#
# Crashes land in .tmp/fuzz/artifacts/<target>/, and the growing corpus in
# .tmp/fuzz/corpus/<target>/. Stop it with Ctrl+C; running it again picks up where it left off.

set -e

TARGET=$1
JOBS=${2:-4}
shift 2 || shift $#

ROOT=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/../.." &> /dev/null && pwd )
BUILD=${BUILD_FOLDER:-$ROOT/build_fuzz}
FUZZ=$ROOT/.tmp/fuzz

if [ -z "$TARGET" ]; then
    echo "usage: run_fuzzer.sh <qst|sav|zplay|music|zasm> [jobs] [libFuzzer flags...]"
    exit 1
fi

mkdir -p "$FUZZ/corpus/$TARGET" "$FUZZ/artifacts/$TARGET" "$FUZZ/logs/$TARGET"
SEEDS=$FUZZ/seeds/$TARGET
[ -d "$SEEDS" ] || SEEDS=

# Leaks are not what this is looking for, and the loaders' globals make them noisy.
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:allocator_may_return_null=1}
# Symbolized stacks in the log, for crashes that only happen after earlier inputs (and so may not
# reproduce from the artifact alone).
if [ -z "$ASAN_SYMBOLIZER_PATH" ]; then
    for s in /opt/homebrew/opt/llvm/bin/llvm-symbolizer /usr/local/opt/llvm/bin/llvm-symbolizer "$(command -v llvm-symbolizer)"; do
        if [ -x "$s" ]; then export ASAN_SYMBOLIZER_PATH=$s; break; fi
    done
fi
export ZC_FUZZ=$TARGET
# The zasm target runs its scripts in a game of this quest.
export ZC_FUZZ_ZASM_QST=${ZC_FUZZ_ZASM_QST:-$ROOT/tests/replays/playground/playground.qst}

# The game's working directory must be the build folder (it loads its .dat files from there).
cd "$BUILD"
exec ./zplayer \
    -fork="$JOBS" \
    -ignore_crashes=1 -ignore_timeouts=1 -ignore_ooms=1 \
    -close_fd_mask=1 \
    -timeout=20 \
    -rss_limit_mb=4096 \
    -malloc_limit_mb=2048 \
    -max_len=8388608 \
    -artifact_prefix="$FUZZ/artifacts/$TARGET/" \
    "$@" \
    "$FUZZ/corpus/$TARGET" $SEEDS
