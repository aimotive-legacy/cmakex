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
    adasworks::log::Logger global_logger(adasworks::log::global_tag, argc, argv, AW_TRACE);
    int result = EXIT_SUCCESS;
    log_info("Current directory: \"%s\"", fs::current_path().c_str());

    auto env_cmake_prefix_path = nowide::getenv("CMAKE_PREFIX_PATH");
    maybe<vector<string>> maybe_env_cmake_prefix_path_vector;
    if (env_cmake_prefix_path) {
        // split along os-specific separator
        vector<pair<string, bool>> msgs;
        maybe_env_cmake_prefix_path_vector = cmakex_prefix_path_to_vector(env_cmake_prefix_path);
    }

    try {
        auto cla = process_command_line_1(argc, argv);
        processed_command_line_args_cmake_mode_t pars;
        cmakex_cache_t cmakex_cache;

        tie(pars, cmakex_cache) = process_command_line_2(cla);

        // cmakex_cache may contain new data to the stored cmakex_cache, or
        // in case of a first cmakex call on this binary dir, it is not saved at all
        // We'll save it on the first successful configuration: either after configuring the
        // wrapper
        // project or after configuring the main project

        CHECK(!pars.source_dir.empty());
        if (pars.deps_mode != dm_main_only) {
            deps_recursion_wsp_t wsp;
            vector<string> global_cmake_args;
            for (auto& c : pars.cmake_args) {
                auto pca = parse_cmake_arg(c);
                if (pca.name != "CMAKE_INSTALL_PREFIX")
                    global_cmake_args.emplace_back(c);
            }
            vector<config_name_t> configs;
            configs.reserve(pars.configs.size());
            for (auto& c : pars.configs)
                configs.emplace_back(c);

            log_info("Configuring helper CMake project");
            HelperCmakeProject hcp(pars.binary_dir);
            auto report = hcp.configure(global_cmake_args);
            // after successful configuration the cmake generator setting has been validated and
            // fixed so we're writing out the cmakex cache
            if (report.cmake_prefix_path)
                cmakex_cache.cmakex_prefix_path_vector =
                    cmakex_prefix_path_to_vector(*report.cmake_prefix_path);
            else
                cmakex_cache.cmakex_prefix_path_vector.clear();
            if (maybe_env_cmake_prefix_path_vector)
                cmakex_cache.env_cmakex_prefix_path_vector = *maybe_env_cmake_prefix_path_vector;
            else
                cmakex_cache.env_cmakex_prefix_path_vector.clear();
            write_cmakex_cache_if_dirty(pars.binary_dir, cmakex_cache);

            install_deps_phase_one(pars.binary_dir, pars.source_dir, {}, global_cmake_args, configs,
                                   wsp, cmakex_cache);
            install_deps_phase_two(pars.binary_dir, wsp, !pars.cmake_args.empty() || pars.flag_c,
                                   pars.build_args, pars.native_tool_args);
            log_info("%d dependenc%s %s been processed.", (int)wsp.pkg_map.size(),
                     wsp.pkg_map.size() == 1 ? "y" : "ies",
                     wsp.pkg_map.size() == 1 ? "has" : "have");
            log_info();
        }
        if (pars.deps_mode == dm_deps_and_main) {
            log_info_framed_message(stringf("Building main project"));
            // add deps install dir to CMAKE_PREFIX_PATH
            const string deps_install_dir = cmakex_config_t(pars.binary_dir).deps_install_dir();
            pars.cmake_args =
                cmake_args_prepend_cmake_prefix_path(pars.cmake_args, deps_install_dir);
        }
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
