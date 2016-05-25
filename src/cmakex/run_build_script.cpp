#include "run_build_script.h"

#include <adasworks/sx/check.h>

namespace cmakex {

const char* k_build_script_runner_dirname = "cmakex-build-script-runner-project";
const char* k_default_binary_dirname = "b";

void run_build_script(const cmakex_pars_t& pars)
{
    // Create background cmake project
    // Configure it again with specifying the build script as parameter
    // The background project executes the build script and
    // - records the add_pkg commands
    // - records the cmakex commands
    // Then the configure ends.
    // Process the recorded add_pkg commands. The result of an add_pkg command is
    // the install directory of the project symlinked or copied into the local
    // prefix dir.

    // create source dir
    CHECK(!pars.binary_dir.empty());
    string build_script_runner_source = pars.binary_dir + "/" + k_build_script_runner_dirname;
    string build_script_runner_binary = build_script_runner_source + k_default_binary_dirname;

    vector<string> args;
    args.emplace_back(string("-H") + build_script_runner_source);
    args.emplace_back(string("-B") + build_script_runner_binary);
}
}
