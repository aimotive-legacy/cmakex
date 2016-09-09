#include "build.h"

#include <adasworks/sx/algorithm.h>
#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "installdb.h"
#include "misc_utils.h"
#include "out_err_messages.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;

build_result_t build(string_par binary_dir,
                     string_par pkg_name,
                     string_par pkg_source_dir,
                     const vector<string>& cmake_args_in,
                     config_name_t config,
                     const vector<string>& build_targets,
                     bool force_config_step,
                     const cmakex_cache_t& cmakex_cache,
                     vector<string> build_args,
                     const vector<string>& native_tool_args)
{
    build_result_t build_result;

    cmakex_config_t cfg(binary_dir);
    CHECK(cmakex_cache.valid);

    // cmake-configure step: Need to do it if
    //
    // force_config_step || new_binary_dir || cmakex_cache_tracker indicates changed settings

    string source_dir;
    string pkg_bin_dir_of_config;
    vector<string> cmake_args = cmake_args_in;

    if (pkg_name.empty()) {           // main project
        source_dir = pkg_source_dir;  // cwd-relative or absolute
        pkg_bin_dir_of_config =
            cfg.main_binary_dir_of_config(config, cmakex_cache.per_config_bin_dirs);
        cmake_args = make_sure_cmake_path_var_contains_path(
            pkg_bin_dir_of_config, "CMAKE_PREFIX_PATH", cfg.deps_install_dir(), cmake_args);
        cmake_args = make_sure_cmake_path_var_contains_path(
            cfg.main_binary_dir_of_config(config, cmakex_cache.per_config_bin_dirs),
            "CMAKE_MODULE_PATH", cfg.find_module_hijack_dir(), cmake_args);
    } else {
        source_dir = cfg.pkg_clone_dir(pkg_name);
        if (!pkg_source_dir.empty())
            source_dir += "/" + pkg_source_dir.str();
        pkg_bin_dir_of_config =
            cfg.pkg_binary_dir_of_config(pkg_name, config, cmakex_cache.per_config_bin_dirs);
    }

    const auto cmake_cache_path = pkg_bin_dir_of_config + "/CMakeCache.txt";
    const bool initial_build = !fs::is_regular_file(cmake_cache_path);

    cmake_cache_t cmake_cache;
    if (initial_build)
        remove_cmake_cache_tracker(pkg_bin_dir_of_config);
    else
        cmake_cache = read_cmake_cache(cmake_cache_path);

    bool cmake_build_type_changing = false;
    string cmake_build_type_option;
    if (!cmakex_cache.multiconfig_generator) {
        string current_cmake_build_type = map_at_or_default(cmake_cache.vars, "CMAKE_BUILD_TYPE");

        if (config.is_noconfig()) {
            if (!current_cmake_build_type.empty()) {
                cmake_build_type_option = "-UCMAKE_BUILD_TYPE";
                cmake_build_type_changing = true;
            }
        } else {
            auto target_cmake_build_type = config.get_prefer_empty();
            if (current_cmake_build_type != target_cmake_build_type) {
                cmake_build_type_option = "-DCMAKE_BUILD_TYPE=" + target_cmake_build_type;
                cmake_build_type_changing = true;
            }
        }
    }  // if multiconfig generator

    force_config_step = force_config_step || initial_build;

    // clean first handled specially
    // 1. it should be applied only once per config
    // 2. sometimes it should be applied automatically
    bool clean_first = is_one_of("--clean-first", build_args);
    bool clean_first_added = false;
    if (clean_first) {
        build_args.erase(
            remove_if(BEGINEND(build_args), [](const string& x) { return x == "--clean-first"; }),
            build_args.end());
    }

    log_info("Writing logs to %s.",
             path_for_log(stringf("%s/%s-%s-*%s", cfg.cmakex_log_dir().c_str(),
                                  config.get_prefer_NoConfig().c_str(), pkg_name.c_str(),
                                  k_log_extension))
                 .c_str());

    {  // scope only
        auto cct = load_cmake_cache_tracker(pkg_bin_dir_of_config);
        cct.add_pending(cmake_args);
        fs::create_directories(pkg_bin_dir_of_config);
        save_cmake_cache_tracker(pkg_bin_dir_of_config, cct);

        // do config step only if needed
        if (force_config_step || !cct.pending_cmake_args.empty()) {
            if (cmake_build_type_changing && !initial_build) {
                clean_first_added = !clean_first;
                clean_first = true;
            }

            auto cmake_args_to_apply = cct.pending_cmake_args;
            if (!cmake_build_type_option.empty())
                cmake_args_to_apply.emplace_back(cmake_build_type_option);

            append_inplace(
                cmake_args_to_apply,
                vector<string>({string("-H") + source_dir, string("-B") + pkg_bin_dir_of_config}));

            auto cl_config = log_exec("cmake", cmake_args_to_apply);

            int r;
            if (pkg_name.empty()) {
                r = exec_process("cmake", cmake_args_to_apply);
            } else {
                OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
                try {
                    r = exec_process("cmake", cmake_args_to_apply, oeb.stdout_callback(),
                                     oeb.stderr_callback());
                } catch (...) {
                    if (g_verbose)
                        log_error("Exception during executing 'cmake' config-step.");
                    r = ECANCELED;
                    fflush(stdout);
                    save_log_from_oem(
                        cl_config, r, oeb.move_result(), cfg.cmakex_log_dir(),
                        stringf("%s-%s-configure%s", pkg_name.c_str(),
                                config.get_prefer_NoConfig().c_str(), k_log_extension));
                    fflush(stdout);
                    throw;
                }
                auto oem = oeb.move_result();

                save_log_from_oem(cl_config, r, oem, cfg.cmakex_log_dir(),
                                  stringf("%s-%s-configure%s", pkg_name.c_str(),
                                          config.get_prefer_NoConfig().c_str(), k_log_extension));
            }
            if (r != EXIT_SUCCESS) {
                if (initial_build && fs::is_regular_file(cmake_cache_path))
                    fs::remove(cmake_cache_path);
                throwf("CMake configure step failed, result: %d.", r);
            }

            // if cmake_args_to_apply is empty then this may not be executed
            // but if it's empty there are no pending variables so the cmake_config_ok
            // will not be missing
            cct.confirm_pending();
            save_cmake_cache_tracker(pkg_bin_dir_of_config, cct);

            // after successful configuration the cmake generator setting has been validated and
            // fixed so we can write out the cmakex cache if it's dirty
            // when processing dependencies the cmakex cache has already been written out after
            // configuring the helper project
            write_cmakex_cache_if_dirty(binary_dir, cmakex_cache);
        }
    }

    for (auto& target : build_targets) {
        vector<string> args = {"--build", pkg_bin_dir_of_config.c_str()};
        if (!target.empty()) {
            append_inplace(args, vector<string>({"--target", target.c_str()}));
        }

        if (cmakex_cache.multiconfig_generator) {
            CHECK(!config.is_noconfig());
            append_inplace(args,
                           vector<string>({"--config", config.get_prefer_NoConfig().c_str()}));
        }

        append_inplace(args, build_args);

        // when changing CMAKE_BUILD_TYPE the makefile generators usually fail to update the
        // configuration-dependent things. An automatic '--clean-first' helps
        if (target == "clean")
            clean_first = false;
        else if (clean_first) {
            if (clean_first_added)
                log_warn(
                    "Automatically adding '--clean-first' because CMAKE_BUILD_TYPE is "
                    "changing");
            args.emplace_back("--clean-first");
            clean_first = false;  // add only for the first target
        }

        if (!native_tool_args.empty()) {
            args.emplace_back("--");
            append_inplace(args, native_tool_args);
        }

        string cl_build = log_exec("cmake", args);
        {  // scope only
            int r;
            if (pkg_name.empty()) {
                r = exec_process("cmake", args);
            } else {
                OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
                try {
                    r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
                } catch (...) {
                    if (g_verbose)
                        log_error("Exception during executing 'cmake' build-step.");
                    r = ECANCELED;
                    fflush(stdout);
                    save_log_from_oem(
                        cl_build, r, oeb.move_result(), cfg.cmakex_log_dir(),
                        stringf("%s-%s-build-%s%s", pkg_name.c_str(),
                                config.get_prefer_NoConfig().c_str(),
                                target.empty() ? "all" : target.c_str(), k_log_extension));
                    fflush(stdout);
                    throw;
                }
                auto oem = oeb.move_result();

                save_log_from_oem(
                    cl_build, r, oem, cfg.cmakex_log_dir(),
                    stringf("%s-%s-build-%s%s", pkg_name.c_str(),
                            config.get_prefer_NoConfig().c_str(),
                            target.empty() ? "all" : target.c_str(), k_log_extension));

                if (r == EXIT_SUCCESS && target == "install") {
                    // write out hijack module that tries the installed config module first
                    // collect the config-modules that has been written
                    for (int i = 0; i < oem.size(); ++i) {
                        auto v = split(oem.at(i).text, '\n');
                        for (auto& text : v) {
                            text = trim(text);
                            auto colon_pos = text.find(':');
                            if (colon_pos == string::npos)
                                continue;
                            auto path_str = trim(text.substr(colon_pos + 1));
                            if (!ends_with(path_str, "-config.cmake") &&
                                !ends_with(path_str, "Config.cmake"))
                                continue;
                            fs::path path(path_str);
                            if (!fs::is_regular_file(path))
                                continue;
                            auto filename = path.filename().string();
                            string base;
                            for (auto e : {"-config.cmake", "Config.cmake"}) {
                                if (ends_with(filename, e)) {
                                    base = filename.substr(0, filename.size() - strlen(e));
                                    break;
                                }
                            }
                            if (base.empty())
                                continue;
                            // find out if there's such an official config module
                            if (cmakex_cache.cmake_root.empty())
                                continue;
                            auto find_module =
                                cmakex_cache.cmake_root + "/Modules/Find" + base + ".cmake";
                            if (!fs::is_regular_file(find_module))
                                continue;

                            build_result.hijack_modules_needed.emplace_back(base);
                        }
                    }
                }
            }
            if (r != EXIT_SUCCESS)
                throwf("CMake build step failed, result: %d.", r);
        }
    }  // for targets

#if 0
        // test step
        if (pars.flag_t) {
            auto tic = high_resolution_clock::now();
            string step_string =
                stringf("test step%s", config.empty() ? "" : (string(" (") + config + ")").c_str());
            log_info("Begin %s", step_string.c_str());
            fs::current_path(pars.binary_dir);
            vector<string> args;
            if (!config.empty()) {
                args.emplace_back("-C");
                args.emplace_back(config);
            }
            log_exec("ctest", args);
            int r = exec_process("ctest", args);
            if (r != EXIT_SUCCESS)
                throwf("Testing failed with error code %d", r);
            log_info(
                "End %s, elapsed %s", step_string.c_str(),
                sx::format_duration(dur_sec(high_resolution_clock::now() - tic).count()).c_str());
        }
#endif
    if (g_verbose)
        log_info("End of build: %s - %s", pkg_for_log(pkg_name).c_str(),
                 config.get_prefer_NoConfig().c_str());

    std::sort(BEGINEND(build_result.hijack_modules_needed));
    sx::unique_trunc(build_result.hijack_modules_needed);
    return build_result;
}
}
