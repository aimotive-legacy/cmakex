#!/bin/bash -e

if [[ $# == 0 ]]; then
    echo "Using the default CMake generator";
    cmake_gen_opt=""
elif [[ $# == 1 ]]; then
    cmake_gen_opt="-G$1"
    echo "Using the CMake generator: $1";
else
    echo "Usage: make.sh [<cmake-generator>]" >&2
    exit 1
fi

common_opts="-DCMAKE_CXX_STANDARD=11 -DCMAKE_DEBUG_POSTFIX=_d"
deps_install=$PWD/deps/o

eval cmakex_cmake_args=(${CMAKEX_CMAKE_ARGS})

function build_core {
    if [[ -n $CMAKEX_CONFIG_DEV ]]; then
        cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_DEBUG_POSTFIX=_d $opts "${cmakex_cmake_args[@]}" "$cmake_gen_opt" -H$src -B$build
        cmake --build $build --target install --config Debug
        cmake $build -DCMAKE_BUILD_TYPE=Release
        cmake --build $build --target install --config Release
    else
        cmake -DCMAKE_BUILD_TYPE=Release $opts "$cmake_gen_opt" -H$src -B$build
        cmake --build $build --target install --config Release
    fi
}

function build_dep {
    name=$1
    src=deps/$1
    build=deps/$1-build
    build_core
}

opts="$common_opts -DCMAKE_INSTALL_PREFIX=$deps_install -DBUILD_SHARED_LIBS=0"\
" -DENABLE_XML=0 -DENABLE_JSON=0 -DENABLE_MONGODB=0 -DENABLE_UTIL=0"\
" -DENABLE_NET=0 -DENABLE_NETSSL=0 -DENABLE_NETSSL_WIN=0 -DENABLE_CRYPTO=0"\
" -DENABLE_DATA=0 -DENABLE_DATA_SQLITE=0 -DENABLE_DATA_MYSQL=0"\
" -DENABLE_DATA_ODBC=0 -DENABLE_SEVENZIP=0 -DENABLE_ZIP=0"\
" -DENABLE_PAGECOMPILER=0 -DENABLE_PAGECOMPILER_FILE2PAGE=0 -DPOCO_STATIC=1"\
" -DENABLE_MSVC_MP=1 -DJUST_INSTALL_CEREAL=1";

if [[ -n $CMAKEX_BUILD_TESTING ]]; then
    opts="$opts -DYAML_CPP_BUILD_TOOLS=1 -DBUILD_TESTING=1 -DENABLE_TESTING=1"
else
    opts="$opts -DYAML_CPP_BUILD_TOOLS=0 -DBUILD_TESTING=0 -DENABLE_TESTING=0"
fi

export MAKEFLAGS=-j8

build_dep aw-sx
build_dep yaml-cpp
build_dep nowide
build_dep Poco
build_dep cereal

opts="$common_opts -DCMAKE_PREFIX_PATH=$deps_install -DCMAKE_INSTALL_PREFIX=$PWD/o" 
src=.
build=b

build_core
