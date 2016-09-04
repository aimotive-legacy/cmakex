# cmakex

Extension for the `cmake` command

Keywords: lightweight C/C++ package management, multiple repos tool,
applying common cmake build/toolchain options, package server

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

Execute multiple `cmake` commands with concise syntax.

    'c', 'b', 'i', 't': perform CMake configure/build/install/test steps
    'd', 'r', 'w':      for Debug, Release, RelWithDebInfo configurations

The letters can be mixed, order is not important.

#### &lt;source/build-dir-spec&gt; is one of

    `-H <path-to-source> -B <path-to-build>`
    `<path-to-existing-build>`
    `-B <path-to-existing-build>`
    `<path-to-source>` (build directory is the current working directory)

`-H` and `-B` options can be written with or without space.

#### Accepted &lt;cmake-args&gt;:

    --config <cfg>: For specifying configs other than Debug, Release, RelWithDebInfo.
                    Can be specified multiple times.
    --target <tgt>: For specifying targets other than ALL (default) and INSTALL (use 'i' in <command>)
                    Can be specified multiple times.
    --clean-first
    -C, -D, -U, -G, -T, -A
    -N, all the -W* options
    --debug-trycompile, --debug-output, --trace, --trace-expand
    --warn-uninitialized, --warn-unused-vars, --no-warn-unused-cli,
    --check-system-vars, --graphwiz=

### Package-management

    --deps
    --deps=<path>      Before executing the cmake-steps on the main project, process the dependency-
                       script at <source-dir>/deps.cmake or <path> and download/configure/build/
                       install the packages defined in the script, if needed
    --deps-only
    --deps-only=<path> Same as `--deps` but does not processes the main project.
                       Note: use `cmakex -B <path-to-new-or-existing-build> --deps-only=<path> ...`
                       to build a list of packages without a main project
    --force-build      Configure and build each dependency even if no options/dependencies have been
                       changed for a package.
    --update-includes  The CMake `include()` command used in the dependency scripts can include a
                       URL. The file the URL refers to will be downloaded and included with the
                       normal `include` command. Further runs will use the local copy. Use this
                       option to purge the local copies and download the files again.

### Presets

    -p <path>#preset[#preset]...
                       Load the YAML file from <path> and add the args defined for the presets to
                       the current command line.
    -p preset[#preset]...
                       Use the file specified in the CMAKEX_PRESET_FILE environment variable.

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
