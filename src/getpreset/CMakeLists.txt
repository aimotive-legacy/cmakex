add_executable(getpreset getpreset.cpp)

if(BUILD_TESTING)
    add_executable(test_getpreset getpreset.cpp)
    target_compile_definitions(test_getpreset PRIVATE GETPRESET_DO_TEST)
    add_test(NAME test_getpreset COMMAND test_getpreset)
endif()

foreach(t getpreset test_getpreset)
    if(TARGET ${t})
        set_target_properties(${t} PROPERTIES
            DEBUG_POSTFIX _d)

        target_link_libraries(${t} PRIVATE
            ::aw-sx yaml-cpp)
    endif()
endforeach()


install(TARGETS getpreset DESTINATION bin)

