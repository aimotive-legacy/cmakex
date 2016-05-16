add_executable(cmakex
    cmakex.cpp
    cmakexengine.h cmakexengine.cpp
    using-decls.h
)

target_link_libraries(cmakex PRIVATE
    ::aw-sx nowide::nowide-static filesystem
    ::tiny-process-library)
