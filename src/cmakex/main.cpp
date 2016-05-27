#include <adasworks/sx/log.h>
#include <nowide/args.hpp>

#include "process_command_line.h"
#include "run_add_pkgs.h"
#include "run_build_script.h"
#include "run_cmake_steps.h"

#include "git.h"

namespace cmakex {
int main(int argc, char* argv[])
{
    nowide::args nwa(argc, argv);

    auto r = git_ls_remote("https://tamas.kenez@scm.adasworks.com/r/frameworks/cmakex.git", "HEAD");

    fprintf(stderr, "%d, %s\n", std::get<0>(r), std::get<1>(r).c_str());
    exit(0);
    adasworks::log::Logger global_logger(adasworks::log::global_tag, argc, argv, AW_TRACE);
    try {
        auto pars = process_command_line(argc, argv);
        if (!pars.add_pkgs.empty())
            run_add_pkgs(pars);
        else if (pars.source_desc_kind == source_descriptor_build_script)
            run_build_script(pars);
        else
            run_cmake_steps(pars);
    } catch (const exception& e) {
        LOG_FATAL("Exception: %s", e.what());
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
