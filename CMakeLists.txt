cmake_minimum_required(VERSION 3.1)
project(cmakex)

include(CMakePrintHelpers)

find_package(aw-sx REQUIRED)
find_package(yaml-cpp REQUIRED)

if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
endif()

add_subdirectory(src)
