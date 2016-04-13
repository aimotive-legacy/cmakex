#!/bin/bash -xe

# Builds and installs the project to ./bin
# Run from the repo root

rm -rf c
mkdir c
pushd c
export MAKEFLAGS=-j8
git clone --depth 1 https://scm.adasworks.com/r/frameworks/adasworks-std-extras.git aw-sx
git clone --depth 1 https://github.com/adasworks/yaml-cpp.git yaml-cpp
cmake -Haw-sx -Bb/aw-sx -DCMAKE_INSTALL_PREFIX=$PWD/o \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=0 -DCMAKE_CXX_STANDARD=11 \
     -DBUILD_TESTING=0
cmake -Hyaml-cpp -Bb/yaml-cpp -DCMAKE_INSTALL_PREFIX=$PWD/o \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=11 \
    -DYAML_CPP_BUILD_TOOLS=0
cmake --build b/aw-sx --target install --config Release
cmake --build b/yaml-cpp --target install --config Release
cmake -H.. -Bb/cmakex -DCMAKE_INSTALL_PREFIX=$PWD/o \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=11
cmake --build b/cmakex --target install --config Release
popd
mkdir -p bin
cp c/o/bin/getpreset bin
rm -rf c
