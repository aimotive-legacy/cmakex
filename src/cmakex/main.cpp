#include <nowide/args.hpp>

#include <cmath>

#include <nowide/cstdlib.hpp>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "git.h"
#include "helper_cmake_project.h"
#include "install_deps_phase_one.h"
#include "install_deps_phase_two.h"
#include "misc_utils.h"
#include "print.h"
#include "process_command_line.h"
#include "run_add_pkgs.h"
#include "run_cmake_steps.h"

namespace cmakex {

namespace fs = filesystem;

int main(int argc, char* argv[])
{
    nowide::args nwa(argc, argv);

#if 0
    {
        const char* x = "./";
        Poco::Path p(x);
        for (int i = 0; i <= p.depth(); ++i) {
            printf("%d: \"%s\"\n", i, p[i].c_str());
        }
        auto y = fs::lexically_normal(x);
        printf("\"%s\" -> \"%s\"\n", x, y.c_str());
        exit(0);
    }
#endif
    adasworks::log::Logger global_logger(adasworks::log::global_tag, AW_TRACE);
    int result = EXIT_SUCCESS;

    for (int argix = 1; argix < argc; ++argix) {
        string arg = argv[argix];
        if (arg == "-V")
            g_verbose = true;
    }

    if (g_verbose) {
        log_info("Current directory: \"%s\"", fs::current_path().c_str());
        for (int i = 0; i < argc; ++i) {
            log_info("arg#%d: `%s`", i, argv[i]);
        }
    }

    auto env_cmake_prefix_path = nowide::getenv("CMAKE_PREFIX_PATH");
    maybe<vector<string>> maybe_env_cmake_prefix_path_vector;
    if (env_cmake_prefix_path) {
        // split along os-specific separator
        vector<pair<string, bool>> msgs;
        maybe_env_cmake_prefix_path_vector = cmakex_prefix_path_to_vector(env_cmake_prefix_path);
    }

    try {
        auto cla = process_command_line_1(argc, argv);
        if (cla.subcommand.empty())
            exit(EXIT_SUCCESS);
        processed_command_line_args_cmake_mode_t pars;
        cmakex_cache_t cmakex_cache;

        tie(pars, cmakex_cache) = process_command_line_2(cla);

        // cmakex_cache may contain new data to the stored cmakex_cache, or
        // in case of a first cmakex call on this binary dir, it is not saved at all
        // We'll save it on the first successful configuration: either after configuring the
        // wrapper
        // project or after configuring the main project

        CHECK(pars.deps_mode == dm_deps_only || !pars.source_dir.empty());
        if (pars.deps_mode != dm_main_only) {
            deps_recursion_wsp_t wsp;
            wsp.force_build = pars.force_build;
            wsp.clear_downloaded_include_files = pars.clear_downloaded_include_files;
            // command_line_cmake_args contains the cmake args specified in the original cmakex call
            // except any arg dealing with CMAKE_INSTALL_PREFIX
            vector<string> command_line_cmake_args;
            for (auto& c : pars.cmake_args) {
                auto pca = parse_cmake_arg(c);
                if (pca.name != "CMAKE_INSTALL_PREFIX") {
                    command_line_cmake_args.emplace_back(c);
                }
            }
            command_line_cmake_args = normalize_cmake_args(command_line_cmake_args);

            vector<config_name_t> configs;
            configs.reserve(pars.configs.size());
            for (auto& c : pars.configs)
                configs.emplace_back(c);

            log_info("Configuring helper CMake project");
            HelperCmakeProject hcp(pars.binary_dir);
            hcp.configure(command_line_cmake_args);

            // get some variables from helper project cmake cache
            auto& cc = hcp.cmake_cache;

            // remember CMAKE_ROOT
            if (cc.vars.count("CMAKE_ROOT") > 0)
                cmakex_cache.cmake_root = cc.vars.at("CMAKE_ROOT");

            // remember CMAKE_PREFIX_PATH
            if (cc.vars.count("CMAKE_PREFIX_PATH") > 0) {
                cmakex_cache.cmakex_prefix_path_vector =
                    cmakex_prefix_path_to_vector(cc.vars.at("CMAKE_PREFIX_PATH"));
            } else
                cmakex_cache.cmakex_prefix_path_vector.clear();

            // remember ENV{CMAKE_PREFIX_PATH}
            if (maybe_env_cmake_prefix_path_vector)
                cmakex_cache.env_cmakex_prefix_path_vector = *maybe_env_cmake_prefix_path_vector;

            // after successful configuration of the helper project the cmake generator setting has
            // been validated and fixed so we're writing out the cmakex cache
            write_cmakex_cache_if_dirty(pars.binary_dir, cmakex_cache);

            fs::create_directories(cmakex_config_t(pars.binary_dir).find_module_hijack_dir());

            install_deps_phase_one(
                pars.binary_dir, pars.source_dir, {}, command_line_cmake_args, configs, wsp,
                cmakex_cache,
                pars.deps_script.empty() ? "" : fs::absolute(pars.deps_script).c_str());
            install_deps_phase_two(pars.binary_dir, wsp, !pars.cmake_args.empty() || pars.flag_c,
                                   pars.build_args, pars.native_tool_args);
            log_info("%d dependenc%s %s been processed.", (int)wsp.pkg_map.size(),
                     wsp.pkg_map.size() == 1 ? "y" : "ies",
                     wsp.pkg_map.size() == 1 ? "has" : "have");
            log_info();
        }
        if (pars.deps_mode == dm_deps_and_main)
            log_info_framed_message(stringf("Building main project"));

        if (pars.deps_mode != dm_deps_only)
            run_cmake_steps(pars, cmakex_cache);
    } catch (const exception& e) {
        log_fatal("%s", e.what());
        result = EXIT_FAILURE;
    } catch (...) {
        log_fatal("Unhandled exception");
        result = EXIT_FAILURE;
    }
    return result;
}
}

int main(int argc, char* argv[])
{
    return cmakex::main(argc, argv);
}
