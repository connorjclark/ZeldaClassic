#!/usr/bin/env bash

set -euxo pipefail

if [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
    xvfb-run --auto-servernum sudo -E python -Xutf8 tests/run_replay_tests.py "$@"
else
    python -Xutf8 tests/run_replay_tests.py "$@"
fi
