#!/bin/bash -e

common_opts="-DCMAKE_CXX_STANDARD=11"
deps_install=$PWD/deps/o

eval cmakex_cmake_args=(${CMAKEX_CMAKE_ARGS})

function build_core {
    if [[ -n $CMAKEX_CONFIG_DEV ]]; then
        cmake -H$src -B$build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_DEBUG_POSTFIX=_d $opts "${cmakex_cmake_args[@]}"
        cmake --build $build --target install --config Debug
        cmake $build -DCMAKE_BUILD_TYPE=Release
        cmake --build $build --target install --config Release
    else
        cmake -H$src -B$build -DCMAKE_BUILD_TYPE=Release $opts
        cmake --build $build --target install --config Release
    fi
}

function build_dep {
    name=$1
    src=deps/$1
    build=deps/$1-build
    build_core
}

opts="$common_opts -DCMAKE_INSTALL_PREFIX=$deps_install -DBUILD_SHARED_LIBS=0" 

if [[ -n $CMAKEX_BUILD_TESTING ]]; then
    opts="$opts -DYAML_CPP_BUILD_TOOLS=1 -DBUILD_TESTING=1"
else
    opts="$opts -DYAML_CPP_BUILD_TOOLS=0 -DBUILD_TESTING=0"
fi

build_dep aw-sx
build_dep yaml-cpp

opts="$common_opts -DCMAKE_PREFIX_PATH=$deps_install -DCMAKE_INSTALL_PREFIX=$PWD/o" 
src=.
build=b

build_core
