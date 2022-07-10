#!/usr/bin/env bash

set -euxo pipefail

git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools
export PATH=$PATH:$PWD/depot_tools

mkdir breakpad && cd breakpad
fetch breakpad
cd src
./configure && make
make install
