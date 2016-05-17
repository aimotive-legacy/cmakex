add_executable(cmakex
    cmakex.cpp
    cmakexengine.h cmakexengine.cpp
    using-decls.h
    invoke.cpp invoke.h
    misc_util.h misc_util.cpp
)

target_link_libraries(cmakex PRIVATE
    ::aw-sx nowide::nowide-static filesystem
    ::tiny-process-library)
