add_executable(getpreset getpreset.cpp)
add_library(libgetpreset STATIC getpreset.cpp getpreset.h)

if(BUILD_TESTING)
    add_executable(test_getpreset getpreset.cpp)
    target_compile_definitions(test_getpreset PRIVATE GETPRESET_DO_TEST)
    add_test(NAME test_getpreset COMMAND test_getpreset)
endif()

foreach(t getpreset test_getpreset libgetpreset)
    if(TARGET ${t})
        set_target_properties(${t} PROPERTIES
            DEBUG_POSTFIX _d)

        target_link_libraries(${t}
            PUBLIC ::aw-sx
            PRIVATE yaml-cpp filesystem nowide::nowide-static common)
    endif()
endforeach()

target_include_directories(libgetpreset PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_definitions(libgetpreset PRIVATE LIBGETPRESET)

install(TARGETS getpreset DESTINATION bin)

