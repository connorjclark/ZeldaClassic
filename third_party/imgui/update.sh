#!/bin/bash

set -e
shopt -s extglob

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd "$SCRIPT_DIR"
rm -rf !(update.sh)

[ ! -d 'imgui' ] && git clone https://github.com/ocornut/imgui.git
git -C imgui checkout 6d910d5487d11ca567b61c7824b0c78c569d62f0 # v1.92.5

mkdir src src/backends src/misc/freetype
cp imgui/*.cpp src
cp imgui/*.h src
cp imgui/backends/imgui_impl_allegro5.* src/backends
cp imgui/misc/freetype/imgui_freetype.* src/misc/freetype
cp imgui/LICENSE.txt imgui/docs/README.md .
rm -fr imgui
