cmake_minimum_required(VERSION ${CMAKE_VERSION})
project(test_cmake_steps)

add_executable(exe main.cpp)
target_compile_definitions(exe PRIVATE
    -DCONFIG=$<CONFIG>
    -DDEFINE_THIS=X${DEFINE_THIS}X
)
set_target_properties(exe PROPERTIES DEBUG_POSTFIX d)

install(TARGETS exe DESTINATION bin)