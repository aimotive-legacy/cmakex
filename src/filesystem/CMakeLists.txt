add_library(filesystem STATIC
    filesystem.cpp filesystem.h)
target_link_libraries(filesystem PRIVATE ::aw-sx nowide::nowide-static)
target_include_directories(filesystem PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

