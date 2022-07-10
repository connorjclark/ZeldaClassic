#!/usr/bin/env bash

set -euxo pipefail

git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools
export PATH=$PWD/depot_tools:$PATH

# Why is depot_tool's python locater so busted?
# cp $(which python3) depot_tools/python3.exe

mkdir breakpad && cd breakpad
fetch breakpad
cd src
./configure && make
make install
