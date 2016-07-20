#include <nowide/args.hpp>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

#include "cmakex_utils.h"
#include "git.h"
#include "install_deps_phase_one.h"
#include "install_deps_phase_two.h"
#include "process_command_line.h"
#include "run_add_pkgs.h"
#include "run_cmake_steps.h"

#include <cmath>

namespace cmakex {

int main(int argc, char* argv[])
{
    nowide::args nwa(argc, argv);

    adasworks::log::Logger global_logger(adasworks::log::global_tag, argc, argv, AW_TRACE);
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
            }
            run_cmake_steps(pars);
        }
    } catch (const exception& e) {
        LOG_FATAL("cmakex: %s", e.what());
    } catch (...) {
        LOG_FATAL("Unhandled exception");
    }
    return EXIT_SUCCESS;
}
}

int main(int argc, char* argv[])
{
    return cmakex::main(argc, argv);
}
