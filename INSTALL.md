# Quick Install

    ./configure.sh && ./make.sh

The executable will be installed to `./o/bin/cmakex`. Copy it to a convenient
location (e.g. `/usr/local/bin`) and add it to your `PATH` as necessary.

# Details for Developers

## Environment Variables

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

## Directory Layout

- `./b` and `./o`: cmakex build and install dirs
- `./deps/<name>` and `./deps/<name>-build`: source and build dirs of the
  dependencies
- `./deps/o`: install dir of the dependencies
