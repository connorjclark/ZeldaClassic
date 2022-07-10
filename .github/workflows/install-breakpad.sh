#!/usr/bin/env bash

set -euxo pipefail

# git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools

mkdir breakpad && cd breakpad
cmd.exe /c '..\depot_tools\fetch breakpad'
ls
cd src
ls
cmd.exe /c '..\..\depot_tools\python2-bin\python2 .\src\tools\gyp\gyp_main.py src\client\windows\breakpad_client.gyp'

msbuild src/client/windows/build_all.vcproj -t:rebuild -verbosity:diag -property:Configuration=Release
ls

# ./configure && make
# make install