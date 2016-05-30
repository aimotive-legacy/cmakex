add_executable(cmakex
    run_cmake_steps.h run_cmake_steps.cpp
    using-decls.h
    misc_utils.h misc_utils.cpp
    print.h print.cpp
    main.cpp pars.h
    process_command_line.h process_command_line.cpp
    run_build_script.cpp run_build_script.h
    run_add_pkgs.cpp run_add_pkgs.h
    cmakex_utils.h cmakex_utils.cpp
    git.cpp git.h
    installdb.h installdb.cpp
)

target_link_libraries(cmakex PRIVATE
    ::aw-sx nowide::nowide-static filesystem
    Poco::Foundation
    process)
