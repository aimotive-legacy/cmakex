add_executable(cmakex
    run_cmake_steps.h run_cmake_steps.cpp
    using-decls.h
    out_err_messages.cpp out_err_messages.h
    misc_util.h misc_util.cpp
    exec_process.h exec_process.cpp
    print.h print.cpp
    main.cpp
    process_command_line.h process_command_line.cpp
)

target_link_libraries(cmakex PRIVATE
    ::aw-sx nowide::nowide-static filesystem
    ::tiny-process-library
    Poco::Foundation)
