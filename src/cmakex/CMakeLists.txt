add_executable(cmakex
    cmakex.cpp
    cmakexengine.h cmakexengine.cpp
    using-decls.h
    out_err_messages.cpp out_err_messages.h
    misc_util.h misc_util.cpp
    exec_process.h exec_process.cpp
    print.h print.cpp
)

target_link_libraries(cmakex PRIVATE
    ::aw-sx nowide::nowide-static filesystem
    ::tiny-process-library
    Poco::Foundation)
