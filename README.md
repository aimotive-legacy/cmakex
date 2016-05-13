# cmakex

Execute multiple CMake steps in one command, define build option presets in a
yaml file.

## Building and Installing

Prerequisites: CMake version 3.1 (this could be lowered on demand)

All other dependencies are cloned and built within this project.
For a quick release build:

    ./configure.sh && ./make.sh

# Details for Developers

## Building and Installing

### Environment Variables

There are a few environment variables which control the building and install
processes:

- set `CMAKEX_CONFIG_DEV=1` to do a full clone of the dependencies (instead of
  the default shallow clone) and build the `Debug` config, too
- set `CMAKEX_BUILD_TESTING=1` to enable testing for both the dependencies and
  cmakex
- set `CMAKEX_CMAKE_ARGS` to additional cmake options

Example:

    export CMAKEX_CMAKE_ARGS="-G \"Kate - Unix Makefiles\""
    CMAKEX_CONFIG_DEV=1 ./configure.sh && ./make.sh

### Directory Layout

- `./b` and `./o`: cmakex build and install dirs
- `./deps/<name>` and `./deps/<name>-build`: source and build dirs of the
  dependencies
- `./deps/o`: install dir of the dependencies
