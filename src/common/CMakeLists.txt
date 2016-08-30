add_library(common STATIC
    using-decls.h
    misc_utils.cpp misc_utils.h)
target_link_libraries(common
        PRIVATE nowide::nowide-static Poco::Foundation
        PUBLIC ::aw-sx
    )
target_include_directories(common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

