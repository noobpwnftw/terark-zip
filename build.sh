#!/bin/bash

BASE=$PWD
OUTPUT=output
mkdir -p $OUTPUT

git submodule update --init --recursive

cd $BASE/$OUTPUT && cmake ../ -DCMAKE_BUILD_TYPE=Release
cd $BASE/$OUTPUT && make -j $(nproc) && make install > /dev/null 2>&1
