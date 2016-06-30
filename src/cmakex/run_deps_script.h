#ifndef RUN_BUILD_SCRIPT_239874
#define RUN_BUILD_SCRIPT_239874

#include "pars.h"

namespace cmakex {

void run_deps_script(string binary_dir,
                     string deps_script_file,
                     const vector<string>& config_args,
                     const vector<string>& configs,
                     bool strict_commits);
}

#endif
