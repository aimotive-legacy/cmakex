# cmakex

Extension for the `cmake` command

Keywords: lightweight, non-intrusive C/C++ package management, multiple repos tool,
applying common cmake build/toolchain options, package server

## Show me quickly what I can do with this

Say, you have a CMake-enabled project which depends on
[jsoncpp](https://github.com/open-source-parsers/jsoncpp) and
[GTest](https://github.com/google/googletest) and on another inhouse library of
yours. Simply create a cmake file `deps.cmake` next to your project's
`CMakeLists.txt` with this content:

```CMake
add_pkg(GTest GIT_URL https://github.com/google/googletest
  CMAKE_ARGS -Dgtest_force_shared_crt=1)
add_pkg(jsoncpp
  GIT_URL https://github.com/open-source-parsers/jsoncpp GIT_TAG 1.7.1
  CMAKE_ARGS -DJSONCPP_WITH_TESTS=0 -DJSONCPP_WITH_POST_BUILD_UNITTEST=0
    -DJSONCPP_WITH_CMAKE_PACKAGE=1)
add_pkg(mylibfoo GIT_URL https://github.com/thatsme/mylibfoo)
```

Then build your project from scratch with this one-liner:

    cmakex br -H myproj -B myproj-build --deps

That single line downloads and installs the dependencies and [b]uilds the [r]elease configuration of your project.

Edit your main project or mylibfoo (or any other dependency) and rebuild:

    cmakex br myproj-build

## Contents

- Overview
- Building and Installing
- Getting Started
- Usage
- Tutorial

## Overview

`cmakex` is a command-line tool, which extends the functionality of the `cmake`
command adding these interdependent features:

1. **Multiple `cmake`-commands with concise syntax**: For example, configure,
   install, test a project for debug & release configs with a single command.
2. **Preset management**: Define common CMake option groups (presets) in a `YAML`
   file. A preset can be a set of cmake-generator settings, platform and
   toolchain options. It can be added to the current command line with
   a single argument.
3. **Lightweight package-management**: Describe your projects' dependencies in a
   cmake-script with `ExternalProject_Add`-like syntax. The dependencies will
   be automatically downloaded and installed before building your main project.
   No change needed in your CMakeLists.txt and you can use unchanged thirdparties
   as dependencies.
4. **Multiple-repos**: Work in multiple projects simultaneously and build only
   the out-of-date projects automatically.
5. **Binary package server**: (to be implemented) Install a local or remote build-
   server which builds, caches and provides installable packages, automatically
   managing the full dependency graph, keeping track of versions and build
   settings.

And all this is

- written in C++ and provided in a self-contained, easy-to-install project.
- as lightweight as it gets: you don't need to abandon your current workflow,
  use only as many features as you need, easy to opt-out.
- open-source, permissive license
- supports a wide range of platforms, UNIX/Mac/Windows

## Building and Installing

Prerequisites: CMake version 3.1 (this could be lowered on demand)

All other dependencies are cloned and built within this project.
For a quick build execute these commands: (use git-bash on Windows)

    ./configure.sh && ./make.sh

## Getting Started

...

## Usage

    cmakex [--help] [--version]
           [c][b][i][t][d][r][w] [<source/build-dir-spec]
           [<cmake-args>...] [<additional-args>...]
           [-- <native-build-tool-args>...]

Note: Invoke CTest is not yet implemented.

### General commands:

    --help       display this message
    --version    display version information
    -V           verbose

### CMake-wrapper mode:

Execute multiple CMake commands with concise syntax.

The command-word consisting of the letters c, b, i, t, d, r, w specifies the
CMake steps to execute and the configurations.

Use one or more of:

    c, b, i, t: to perform CMake configure/build/install/test steps
       d, r, w: for Debug, Release and RelWithDebInfo configurations

The order of the letters is not important.

#### &lt;source/build-dir-spec&gt; is one of

    -H <path-to-source> -B <path-to-build>
    <path-to-existing-build>
    -B <path-to-existing-build>
    <path-to-source> (build directory is the current working directory)

`-H` and `-B` options can be written with or without spaces.

#### Accepted &lt;cmake-args&gt;:

For detailed help on these arguments see the documentation of the `cmake`
command: [cmake(1)](https://cmake.org/cmake/help/latest/manual/cmake.1.html).

    --config <cfg>: For specifying configs other than Debug, Release
                    RelWithDebInfo. Can be used multiple times.
    --target <tgt>: For specifying targets other than ALL (default) and INSTALL
                    (for that one, use 'i' in the command-word). Can be used
                    multiple times.
    --clean-first
    -C, -D, -U, -G, -T, -A
    -N, all the -W* options
    --debug-trycompile, --debug-output, --trace, --trace-expand
    --warn-uninitialized, --warn-unused-vars, --no-warn-unused-cli,
    --check-system-vars, --graphwiz=

### Package-management


    --deps[=<path>]
                  Before executing the cmake-steps on the main project, process
                  the dependency-script at <source-dir>/deps.cmake (default) or
                  at <path> and download/configure/build/install the packages
                  defined in the script (on demand)
    --deps-only[=<path>]
                  Same as `--deps` but does not process the main project.
                  Tip: use `cmakex -B <path-to-new-or-existing-build>
                  --deps-only=<path> ...` to build a list of packages without a
                  main project
    --force-build Configure and build each dependency even if no build options
                  or dependencies have been changed for a package.
    --update[=MODE]
                  The update operation tries to set the previously cloned repos
                  of the dependencies to the state as if they were freshly cloned.
                  The MODE option can be `if-clean`, `if-very-clean`, `all-clean`
                  and `all-very-clean`.
                  If there are local changes, no updates will be done.
                  Additionally, `*-clean` modes may leave the current branch if
                  needed, while the `*-very-clean` modes don't update if the
                  current branch should be left to update.
                  The `if-*` modes skip the operation if the update is not
                  possible while the `all-*` modes halt with an error message.
                  The default MODE is 'all-very-clean`, the most safe mode.
    --update-includes
                  The CMake `include()` command used in the dependency scripts
                  can include a URL. The file the URL refers to will be
                  downloaded and included with the normal `include` command.
                  Further runs will use the local copy. Use this option to purge
                  the local copies and download the files again.

### Presets

    -p <path>#preset[#preset]...
                  Load the YAML file from <path> and add the args defined for
                  the presets to the current command line.
    -p preset[#preset]...
                  Use the file specified in the CMAKEX_PRESET_FILE environment
                  variable.

If the CMAKEX_PRESET_FILE environment variable is not set and there's a
`default-cmakex-presets.yaml` file in the directory of the cmakex executable
it will be used as default preset file.

cmakex configuration
====================

    --deps-source=<DIR>
    --deps-build=<DIR>
    --deps-install=<DIR>
                    Parent directories of the of the source directories (cloned
                    repositories), build directories and install directory for
                    the dependencies. The default values are `_deps`,
                    `_deps-build` and `_deps-install` under `CMAKE_BINARY_DIR`.
    --single-build-dir
                    With makefile-generators (as opposed to multiconfig) the
                    default is to create separate build directories for each
                    configuration (Debug, Release, etc..).
                    Use this option to force a single build directory.
                    Effective only on the initial configuration and only
                    for makefile-generators.

Miscellaneous
=============

    --manifest=<path>
              Save a manifest file to <path> which can be used to reproduce the
              same build later. It contains `add_pkg` commands to describe the
              dependencies and also information about the command line and the
              main project. It's advised to use the `.cmake` extenstion so it
              can be given to the `--deps=` argument later.

### Examples:

Configure, install and test a project from scrach, for `Debug` and `Release`
configurations, clean build:

    cmakex itdr -H . -B b -DCMAKE_INSTALL_PREFIX=$PWD/out -DFOO=BAR

Configure new project, `Debug` config, use a preset:

    cmakex cd -H . -B b -p preset-dir/presets.yaml#android-toolchain

Build `Release` config in existing build dir, with dependencies

    cmakex br my-build-dir --deps

## Tutorial

...
