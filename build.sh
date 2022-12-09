#!/bin/sh

rm -rf build
mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Debug # use Debug for easy debugging with gdb
make -j2 # or ninja, depending on your system
