#!/bin/sh
set -e

touch src/metadata/sigs/devsig.h.sig

# OPTIMIZE_FLAGS="-Oz -s ENVIRONMENT=web" # PRODUCTION
# OPTIMIZE_FLAGS="-g4 --source-map-base http://localhost:8000/" # DEBUG
OPTIMIZE_FLAGS="-s ASSERTIONS=2 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=1 -g4 --source-map-base http://localhost:8000/build/wasm/" # DEBUG

# TODO: only needed for config stuff, but that isn't working yet.
PRELOAD_FILE="--preload-file output/modules/default@/modules"

mkdir -p build/wasm
emcc -o build/wasm/zc.js $OPTIMIZE_FLAGS \
  -Wno-narrowing \
  -DALLEGRO_LITTLE_ENDIAN -D__TIMEZONE__ \
  -DALLEGRO_LINUX -DALLEGRO_UNIX \
  -Iallegro/include -Iinclude/lpng1212 -Iinclude/zlib123 -Iinclude/loadpng -Iinclude/jpgalleg-2.5 \
  -s MODULARIZE -s ALLOW_MEMORY_GROWTH=1 -s LINKABLE=1 -s EXPORT_ALL=1 \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","FS"]' \
  -s TOTAL_MEMORY=40108032 \
  -Wno-macro-redefined \
  $PRELOAD_FILE \
  src/wasm_read_main.cpp src/qst.cpp src/zsys.cpp src/zelda.cpp src/sprite.cpp src/link.cpp src/ffscript.cpp src/items.cpp src/tiles.cpp src/maps.cpp src/defdata.cpp src/subscr.cpp src/guys.cpp src/zc_custom.cpp src/zscriptversion.cpp allegro/src/*.c allegro/src/unix/*.c src/alleg_compat.cpp

node enhance-source-map.js # DEBUG

# $(ls src/*.cpp | grep -Ev "(Get_MapData|Set_Mapdata|LINUX_Console|colourdata)" | xargs -I {} echo {})
