add_library(process STATIC
    exec_process.cpp exec_process.h
    out_err_messages.cpp out_err_messages.h)

target_link_libraries(process PUBLIC ::aw-sx PRIVATE Poco::Foundation)
target_include_directories(filesystem PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

