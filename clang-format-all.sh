#!/bin/bash -e
git ls-files -- '*.cpp' '*.h' | xargs clang-format $1 -i -style=file
