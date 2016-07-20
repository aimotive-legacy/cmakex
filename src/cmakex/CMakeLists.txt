add_executable(cmakex
    run_cmake_steps.h run_cmake_steps.cpp
    using-decls.h
    misc_utils.h misc_utils.cpp
    print.h print.cpp
    main.cpp
    process_command_line.h process_command_line.cpp
    install_deps_phase_one.cpp install_deps_phase_one.h
    install_deps_phase_two.cpp install_deps_phase_two.h
    run_add_pkgs.cpp run_add_pkgs.h
    cmakex_utils.h cmakex_utils.cpp
    git.cpp git.h
    installdb.h installdb.cpp
    circular_dependency_detector.cpp circular_dependency_detector.h
    clone.h clone.cpp
    cmakex-types.h
    build.h build.cpp
)

target_link_libraries(cmakex PRIVATE
    ::aw-sx nowide::nowide-static filesystem
    Poco::Foundation
    cereal
    process
)
