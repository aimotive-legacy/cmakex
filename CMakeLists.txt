cmake_minimum_required(VERSION 3.1)
project(cmakex)

include(CMakePrintHelpers)

find_package(aw-sx REQUIRED)
find_package(yaml-cpp REQUIRED)

add_executable(getpreset src/getpreset.cpp)
add_executable(test_getpreset src/getpreset.cpp)

foreach(t getpreset test_getpreset)
    set_target_properties(${t} PROPERTIES
        DEBUG_POSTFIX _d)

    target_link_libraries(${t} PRIVATE
        ::aw-sx yaml-cpp)
endforeach()

target_compile_definitions(test_getpreset PRIVATE GETPRESET_DO_TEST)

install(TARGETS getpreset DESTINATION bin)

if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
endif()

add_test(NAME test_getpreset COMMAND test_getpreset)
