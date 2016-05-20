macro(aw_update_runtime_path test_target)
    if(MSVC)
        set(path "${CMAKE_INSTALL_PREFIX}/bin")
        set(dll_paths "")
        foreach(d IN LISTS CMAKE_PREFIX_PATH)
            file(GLOB_RECURSE dlls "${d}/*.dll")
            foreach(dll IN LISTS dlls)
                get_filename_component(dll_dir "${dll}" DIRECTORY)
                list(APPEND dll_paths "${dll_dir}")
                list(REMOVE_DUPLICATES dll_paths)
            endforeach()
        endforeach()
        foreach(dll_path IN LISTS dll_paths)
            set(path "${path};${dll_path}")
        endforeach()
        message(STATUS "Adding these DLL paths for test environments:")
        cmake_print_variables(path)
        string(REPLACE ";" "\;" path "PATH=${path};$ENV{PATH}")
        set_tests_properties(${test_target} PROPERTIES ENVIRONMENT "${path}")
    endif()
endmacro()

add_executable(test_cmake_steps test_cmake_steps.cpp)
add_test(NAME test_cmake_steps
    COMMAND test_cmake_steps
        $<TARGET_FILE:cmakex>
        ${CMAKE_CURRENT_SOURCE_DIR}/test_cmake_steps_cmake_project
        ${CMAKE_CURRENT_BINARY_DIR}/test_cmake_steps_cmake_binary
)
target_link_libraries(test_cmake_steps ::aw-sx filesystem process)

aw_update_runtime_path(test_cmake_steps)
