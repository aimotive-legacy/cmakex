#include "build.h"

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "installdb.h"
#include "misc_utils.h"
#include "out_err_messages.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;

void build(string_par binary_dir,
           const pkg_desc_t& request,
           string_par config,
           bool force_config_step)
{
    log_info("Configuration: %s", same_or_NoConfig(config).c_str());

    cmakex_config_t cfg(binary_dir);

    // cmake-configure step: Need to do it if
    //
    // force_config_step || new_binary_dir || cmakex_cache_tracker indicates changed settings

    auto pkg_clone_dir = cfg.pkg_clone_dir(request.name);
    string source_dir = pkg_clone_dir;
    if (!request.b.source_dir.empty())
        source_dir += "/" + request.b.source_dir;

    CHECK(cfg.cmakex_cache_loaded());
    string pkg_bin_dir_of_config =
        cfg.pkg_binary_dir_of_config(request.name, config, cfg.cmakex_cache().per_config_bin_dirs);

    vector<string> cmake_args = request.b.cmake_args;

    // check if there's no install_prefix
    for (auto& c : cmake_args) {
        if (!starts_with(c, "-DCMAKE_INSTALL_PREFIX"))
            continue;
        auto pca = parse_cmake_arg(c);
        if (pca.name == "CMAKE_INSTALL_PREFIX") {
            throwf(
                "Internal error: global and package cmake_args should not change "
                "CMAKE_PREFIX_PATH: '%s'",
                c.c_str());
        }
    }

    cmake_args.emplace_back(stringf("-DCMAKE_INSTALL_PREFIX=%s", cfg.deps_install_dir().c_str()));

    if (!cfg.cmakex_cache().multiconfig_generator && !config.empty())
        cmake_args.emplace_back(stringf("-DCMAKE_BUILD_TYPE=%s", config.c_str()));

    force_config_step =
        force_config_step || !fs::is_regular_file(pkg_bin_dir_of_config + "/CMakeCache.txt");

    {  // scope only

        CMakeCacheTracker cct(pkg_bin_dir_of_config);
        vector<string> cmake_args_to_apply;
        if (cfg.cmakex_cache().per_config_bin_dirs) {
            // apply and confirm args here and use it for new bin dirs
            auto pkg_bin_dir_common = cfg.pkg_binary_dir_common(request.name);
            CMakeCacheTracker ccc(pkg_bin_dir_common);
            ccc.cmake_config_ok();
            cmake_args_to_apply = cct.about_to_configure(normalize_cmake_args(cmake_args),
                                                         force_config_step, pkg_bin_dir_common);
        } else
            cmake_args_to_apply =
                cct.about_to_configure(normalize_cmake_args(cmake_args), force_config_step);

        if (force_config_step || !cmake_args_to_apply.empty()) {
            cmake_args_to_apply.insert(
                cmake_args_to_apply.begin(),
                {string("-H") + source_dir, string("-B") + pkg_bin_dir_of_config});

            log_exec("cmake", cmake_args_to_apply);
            OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
            int r = exec_process("cmake", cmake_args_to_apply, oeb.stdout_callback(),
                                 oeb.stderr_callback());
            auto oem = oeb.move_result();

            save_log_from_oem("CMake-configure", r, oem, cfg.cmakex_log_dir(),
                              stringf("%s-%s-configure%s", request.name.c_str(), config.c_str(),
                                      k_log_extension));

            if (r != EXIT_SUCCESS)
                throwf("CMake configure step failed, result: %d.", r);

            // if cmake_args_to_apply is empty then this may not be executed
            // but if it's empty there are no pending variables so the cmake_config_ok
            // will not be missing
            cct.cmake_config_ok();
        }
    }

    vector<string> args{{"--build", pkg_bin_dir_of_config}};

    if (cfg.cmakex_cache().multiconfig_generator) {
        CHECK(!config.empty());
        args.insert(args.end(), {"--config", config.c_str()});
    }

    // todo add build_args
    // todo add native tool args
    // todo clear install dir if --clean-first

    log_exec("cmake", args);
    {
        OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
        int r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
        auto oem = oeb.move_result();

        save_log_from_oem(
            "Build", r, oem, cfg.cmakex_log_dir(),
            stringf("%s-%s-build%s", request.name.c_str(), config.c_str(), k_log_extension));

        if (r != EXIT_SUCCESS)
            throwf("CMake build step failed, result: %d.", r);
    }

    args.assign({"--build", pkg_bin_dir_of_config.c_str(), "--target", "install"});
    if (cfg.cmakex_cache().multiconfig_generator)
        args.insert(args.end(), {"--config", config.c_str()});

    log_exec("cmake", args);
    {
        OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
        int r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
        auto oem = oeb.move_result();

        save_log_from_oem(
            "Install", r, oem, cfg.cmakex_log_dir(),
            stringf("%s-%s-install%s", request.name.c_str(), config.c_str(), k_log_extension));

        if (r != EXIT_SUCCESS)
            throwf("CMake install step failed, result: %d.", r);
    }
}
}
