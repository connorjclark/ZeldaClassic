#!/usr/bin/env bash

set -euxo pipefail

git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools

mkdir breakpad && cd breakpad
cmd.exe /c '..\depot_tools\fetch breakpad'
echo "0---"
ls
echo "1---"
cd src
# I think it returns exit code 1 because the .gyp has cycles?
cmd.exe /c '..\..\depot_tools\python2-bin\python2 .\src\tools\gyp\gyp_main.py src\client\windows\breakpad_client.gyp' || true
echo "2---"
ls
echo "3---"
ls src/client/windows/

echo $(which msbuild)
pwd
msbuild src/client/windows/build_all.vcproj -t:rebuild -verbosity:diag -property:Configuration=Release
ls

# ./configure && make
# make install
