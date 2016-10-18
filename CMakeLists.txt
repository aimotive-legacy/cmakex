cmake_minimum_required(VERSION 3.1)
project(cmakex)

include(CMakePrintHelpers)


set(common_gcc_clang -Wno-shorten-64-to-32 -Wno-sign-compare
    -Wno-sign-conversion -Werror=return-type -Wformat=2 )
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Weverything
        -Wno-c++98-compat -Wno-old-style-cast -Wno-deprecated
        -Wno-c++98-compat-pedantic -Wno-padded
        -Wno-global-constructors -Wno-documentation-unknown-command
        -Wno-implicit-fallthrough -Wno-weak-vtables
        -Wno-shadow -Wno-cast-align
        -Wno-conversion -Wno-double-promotion
        -Wno-covered-switch-default
        ${common_gcc_clang}
        )
elseif(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-Wall -Wextra -Wnon-virtual-dtor -pedantic
        -Wunused -Woverloaded-virtual 
        ${common_gcc_clang})
elseif(MSVC)
    add_compile_options(/W4 /W44640 /w14242 /w14254 /w14263 /w14265 /w14287
        /we4289 /w14296 /w14311 /w14545 /w14546 /w14547 /w14549 /w14555 /w14619
        /w14640 /w14826 /w14905 /w14906 /w14928)
endif()

set(CMAKE_DEBUG_POSTFIX d)
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif()

if(MSVC)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -DVC_EXTRALEAN)
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

string(REGEX MATCH "^[0-9.]*" CMAKEX_VERSION_MMP "${version_string}")
set(CMAKEX_VERSION_WITH_META "${version_string}+${t}.${sha}")
add_definitions(
    -DCMAKEX_VERSION_MMP=${CMAKEX_VERSION_MMP}
    -DCMAKEX_VERSION_WITH_META=${CMAKEX_VERSION_WITH_META}
)

cmake_print_variables(CMAKEX_VERSION_MMP CMAKEX_VERSION_WITH_META)

find_package(aw-sx REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nowide REQUIRED)
find_package(Poco REQUIRED Foundation Util)
find_package(cereal REQUIRED)

if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
endif()

add_subdirectory(src)
add_subdirectory(test)

