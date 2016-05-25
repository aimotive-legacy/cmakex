#include <adasworks/sx/log.h>
#include <nowide/args.hpp>

#include "process_command_line.h"
#include "run_cmake_steps.h"
#include "run_build_script.h"

namespace cmakex {
int main(int argc, char* argv[])
{
    nowide::args nwa(argc, argv);

    adasworks::log::Logger global_logger(adasworks::log::global_tag, argc, argv, AW_TRACE);
    try {
        auto pars = process_command_line(argc, argv);
        if (pars.source_desc_kind == source_descriptor_build_script)
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
