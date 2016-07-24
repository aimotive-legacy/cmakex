#include <nowide/args.hpp>

#include <cmath>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "git.h"
#include "install_deps_phase_one.h"
#include "install_deps_phase_two.h"
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
    try {
        auto pars = process_command_line(argc, argv);
        if (!pars.add_pkgs.empty())
            run_add_pkgs(pars);
        else {
            CHECK(!pars.b.source_dir.empty());
            if (pars.deps) {
                cmakex_config_t cfg(pars.binary_dir);
                deps_recursion_wsp_t wsp;
                install_deps_phase_one(pars.binary_dir, pars.b.source_dir, {}, pars.b.cmake_args,
                                       pars.b.configs, pars.strict_commits, wsp);
                install_deps_phase_two(pars.binary_dir, wsp);
                log_info("");
                log_info("%d dependenc%s %s been processed.", (int)wsp.pkg_map.size(),
                         wsp.pkg_map.size() == 1 ? "y" : "ies",
                         wsp.pkg_map.size() == 1 ? "has" : "have");
                log_info("Building main project.");
                log_info("");
            }
            run_cmake_steps(pars);
        }
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
