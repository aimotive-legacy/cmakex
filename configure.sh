#!/bin/bash -e

function clone {
    name=$1
    url=$2
    dir=deps/$name
    if [[ -d $dir ]]; then
        echo "-- Update $name"
        cd $dir
        git pull --ff-only
        cd -
    else
        echo "-- Clone $name"
        mkdir -p $dir
        if [[ -z $CMAKEX_CONFIG_DEV ]]; then
            depthopt="--depth 1"
        else
            depthopt=
        fi
        git clone $depthopt $3 $4 $url $dir
    fi
}

clone aw-sx https://scm.adasworks.com/r/frameworks/adasworks-std-extras.git
clone yaml-cpp https://github.com/adasworks/yaml-cpp.git
clone nowide https://github.com/adasworks/nowide-standalone.git
clone tiny-process-library https://github.com/adasworks/tiny-process-library.git
clone Poco https://github.com/pocoproject/poco.git --branch master
