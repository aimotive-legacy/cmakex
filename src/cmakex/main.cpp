#include <nowide/args.hpp>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

#include "cmakex_utils.h"
#include "git.h"
#include "process_command_line.h"
#include "run_add_pkgs.h"
#include "run_cmake_steps.h"
#include "run_deps_script.h"

namespace cmakex {

void install(const pkg_request_t& pkg_request)
{
    LOG_INFO("Installing %s", pkg_request.name.c_str());
    // todo
}

int main(int argc, char* argv[])
{
    nowide::args nwa(argc, argv);

    adasworks::log::Logger global_logger(adasworks::log::global_tag, argc, argv, AW_TRACE);
    try {
        auto pars = process_command_line(argc, argv);
        if (!pars.add_pkgs.empty())
            run_add_pkgs(pars);
        else {
            CHECK(!pars.source_dir.empty());
            if (pars.deps) {
                cmakex_config_t cfg(pars.binary_dir, pars.source_dir);
                deps_recursion_wsp_t wsp;
                run_deps_script(pars.binary_dir, cfg.deps_script_file, pars.config_args,
                                pars.configs, pars.strict_commits, wsp);
                for (auto& p : wsp.build_order)
                    install(wsp.pkg_request_map[p]);
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
