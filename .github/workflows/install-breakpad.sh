#!/usr/bin/env bash

set -euxo pipefail

# git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools
setx path "%path%;$PWD\depot_tools"

mkdir crashpad && cd crashpad
fetch crashpad
cd crashpad
gn gen out/Default
ninja -C out/Default

# https://stackoverflow.com/questions/26191004/how-do-i-integrate-google-crashpad-with-my-application

# mkdir breakpad && cd breakpad
# cmd.exe /c '..\depot_tools\fetch breakpad'
# echo "0---"
# ls
# echo "1---"
# cd src
# cmd.exe /c '..\..\depot_tools\python2-bin\python2 .\src\tools\gyp\gyp_main.py --no-circular-check src\client\windows\breakpad_client.gyp'
# echo "2---"
# ls
# echo "3---"
# ls src/client/windows/

# # echo $(which msbuild)
# pwd
# cmd.exe /c 'msbuild src\client\windows\breakpad_client.sln -tv:16.0 -t:rebuild -verbosity:diag -property:Configuration=Release'
# ls

# ./configure && make
# make install
