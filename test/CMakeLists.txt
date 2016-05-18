add_executable(test_cmake_steps test_cmake_steps.cpp)
add_test(NAME test_cmake_steps
    COMMAND test_cmake_steps
        $<TARGET_FILE:cmakex>
        ${CMAKE_CURRENT_SOURCE_DIR}/test_cmake_steps_cmake_project
        ${CMAKE_CURRENT_BINARY_DIR}/test_cmake_steps_cmake_binary
)
target_link_libraries(test_cmake_steps ::aw-sx filesystem process)
