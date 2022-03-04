#!/usr/bin/env bash

export OODLELIB=oo2ext_7_win64.dll

# build oozlin
mkdir -p build
cd build
cmake ../
cd ..
cmake --build build/

# copy oozlin binary and libs to /libs in rootdir
cp build/oozlin .
mkdir -p libs
cp build/linoodle/liblinoodle.so libs/
cp ./${OODLELIB} libs/
