cmake_minimum_required(VERSION 3.1)
project(cmakex)

include(CMakePrintHelpers)

set(CMAKE_DEBUG_POSTFIX d)
set(CMAKE_CXX_STANDARD 11)

set(gcc_opts "-Werror=return-type -Wformat=2 -Wall -Wextra -Wno-sign-compare")

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${gcc_opts}")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${gcc_opts}")
endif()

file(READ "VERSION" version_string)
string(STRIP "${version_string}" version_string)

string(TIMESTAMP t "%y%m%d%H%M" UTC)

set(sha "0")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE r
        OUTPUT_VARIABLE o)
    if(NOT r)
        string(SUBSTRING "${o}" 0 6 sha)
    endif()
endif()

add_definitions(
    -DCMAKEX_VERSION_STRING=${version_string}+${t}.${sha}
)

find_package(aw-sx REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nowide REQUIRED)
find_package(Poco REQUIRED Foundation)
find_package(cereal REQUIRED)

if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
endif()

add_subdirectory(src)
add_subdirectory(test)

