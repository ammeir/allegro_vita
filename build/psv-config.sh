#!/bin/sh

rm -f allegro-config Makefile cmake_install.cmake CMakeCache.txt
cmake \
      -DWANT_JPGALLEG=OFF \
      -DWANT_LOADPNG=OFF \
      -DWANT_LOGG=OFF \
      -DWANT_ALLEGROGL=OFF \
      -DWANT_EXAMPLES=OFF -DWANT_TESTS=OFF -DWANT_TOOLS=OFF -DSHARED=OFF \
      -DCMAKE_BUILD_TYPE="Debug" \
      -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake \
      -DPSVITA="1" ..

