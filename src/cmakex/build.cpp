#include "build.h"

#include <adasworks/sx/algorithm.h>
#include <adasworks/sx/check.h>

#include <Poco/DirectoryIterator.h>
#include <Poco/Glob.h>

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
    test_cmake();

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
            pkg_bin_dir_of_config, "CMAKE_MODULE_PATH", cfg.find_module_hijack_dir(), cmake_args);
        cmake_args = make_sure_cmake_path_var_contains_path(
            pkg_bin_dir_of_config, "CMAKE_PREFIX_PATH", cfg.deps_install_dir(), cmake_args);
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

    force_config_step =
        force_config_step || initial_build || cmake_build_type_changing || !cmake_args.empty();

    // clean first handled specially
    // 1. it should be applied only once per config
    // 2. sometimes it should be applied automatically
    const bool clean_first_was_specified = is_one_of("--clean-first", build_args);
    bool clean_first_is_needed = clean_first_was_specified;
    // always remove it at this stage, we'll add it later if needed
    if (clean_first_was_specified) {
        build_args.erase(
            remove_if(BEGINEND(build_args), [](const string& x) { return x == "--clean-first"; }),
            build_args.end());
    }

    log_info("Writing logs to %s.",
             path_for_log(stringf("%s/%s-%s-*%s", cfg.cmakex_log_dir().c_str(), pkg_name.c_str(),
                                  config.get_prefer_NoConfig().c_str(), k_log_extension))
                 .c_str());

    const auto pipe_mode = g_supress_deps_cmake_logs ? pipe_capture : pipe_echo_and_capture;
    {  // scope only
        auto cct = load_cmake_cache_tracker(pkg_bin_dir_of_config);
        cct.add_pending(cmake_args);
        fs::create_directories(pkg_bin_dir_of_config);
        save_cmake_cache_tracker(pkg_bin_dir_of_config, cct);

        // do config step only if needed
        if (force_config_step || !cct.pending_cmake_args.empty()) {
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
                OutErrMessagesBuilder oeb(pipe_mode, pipe_mode);
                try {
                    r = exec_process("cmake", cmake_args_to_apply, oeb.stdout_callback(),
                                     oeb.stderr_callback());
                } catch (...) {
                    if (g_verbose)
                        log_error("Exception during executing 'cmake' config-step.");
                    r = ECANCELED;
                    fflush(stdout);
                    save_log_from_oem(
                        cl_config, r != EXIT_SUCCESS, oeb.move_result(), cfg.cmakex_log_dir(),
                        stringf("%s-%s-configure%s", pkg_name.c_str(),
                                config.get_prefer_NoConfig().c_str(), k_log_extension));
                    fflush(stdout);
                    throw;
                }
                auto oem = oeb.move_result();

                save_log_from_oem(cl_config, r != EXIT_SUCCESS, oem, cfg.cmakex_log_dir(),
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

    if ((cmake_build_type_changing && !initial_build) || clean_first_was_specified)
        clean_first_is_needed = true;

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
            clean_first_is_needed = false;
        else if (clean_first_is_needed) {
            if (!clean_first_was_specified)
                log_warn(
                    "Automatically adding '--clean-first' because CMAKE_BUILD_TYPE is "
                    "changing");
            args.emplace_back("--clean-first");
            clean_first_is_needed = false;  // add only for the first target
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
                OutErrMessagesBuilder oeb(pipe_mode, pipe_mode);
                try {
                    r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
                } catch (...) {
                    if (g_verbose)
                        log_error("Exception during executing 'cmake' build-step.");
                    r = ECANCELED;
                    fflush(stdout);
                    save_log_from_oem(
                        cl_build, r != EXIT_SUCCESS, oeb.move_result(), cfg.cmakex_log_dir(),
                        stringf("%s-%s-build-%s%s", pkg_name.c_str(),
                                config.get_prefer_NoConfig().c_str(),
                                target.empty() ? "all" : target.c_str(), k_log_extension));
                    fflush(stdout);
                    throw;
                }
                auto oem = oeb.move_result();

                save_log_from_oem(
                    cl_build, r != EXIT_SUCCESS, oem, cfg.cmakex_log_dir(),
                    stringf("%s-%s-build-%s%s", pkg_name.c_str(),
                            config.get_prefer_NoConfig().c_str(),
                            target.empty() ? "all" : target.c_str(), k_log_extension));

                if (r == EXIT_SUCCESS && target == "install") {
                    vector<pair_ss> cmake_find_module_names;
                    bool cmake_find_module_names_loaded = false;
                    auto load_cmake_find_module_names = [&cmake_find_module_names, &cmakex_cache,
                                                         &cmake_find_module_names_loaded]() {
                        if (cmake_find_module_names_loaded)
                            return;
                        string find_module_dir = cmakex_cache.cmake_root + "/Modules";
                        string find_module_pattern = find_module_dir + "/Find*.cmake";
                        if (!fs::is_directory(find_module_dir))
                            return;
                        Poco::Glob globber("Find*.cmake");
                        for (Poco::DirectoryIterator it(find_module_dir);
                             it != Poco::DirectoryIterator(); ++it) {
                            if (!it->isFile())
                                continue;
                            if (!globber.match(it.name()))
                                continue;
                            const int c_strlen_Find = 4;
                            string basename =
                                make_string(butleft(it.path().getBaseName(), c_strlen_Find));
                            cmake_find_module_names.emplace_back(tolower(basename), basename);
                        }
                        std::sort(BEGINEND(cmake_find_module_names));
                        cmake_find_module_names_loaded = true;
                    };

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
                            load_cmake_find_module_names();
                            tolower_inplace(base);
                            auto it = std::lower_bound(
                                BEGINEND(cmake_find_module_names), tolower(base),
                                [](const pair_ss& x, const string& y) { return x.first < y; });
                            if (it->first == base)
                                build_result.hijack_modules_needed.emplace_back(it->second);
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
