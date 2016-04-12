cmake_minimum_required(VERSION 3.1)
project(cmakex)

find_package(aw-sx REQUIRED)
find_package(yaml-cpp REQUIRED)

add_executable(getpreset src/getpreset.cpp)

set_target_properties(getpreset PROPERTIES
    DEBUG_POSTFIX _d)

target_link_libraries(getpreset PRIVATE
    ::aw-sx yaml-cpp)

install(TARGETS getpreset DESTINATION bin)
