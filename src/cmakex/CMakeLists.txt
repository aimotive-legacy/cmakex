add_executable(cmakex
    cmakex.cpp
    cmakexengine.h cmakexengine.cpp
    using-decls.h
    filesystem.cpp filesystem.h
)

target_link_libraries(cmakex PRIVATE ::aw-sx)