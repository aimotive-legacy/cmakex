#ifndef CMAKEX_UTILS_23094
#define CMAKEX_UTILS_23094

#include "pars.h"

namespace cmakex {

struct cmakex_config_t
{
    cmakex_config_t(string_par cmake_binary_dir);

    string cmakex_dir;
    string cmakex_deps_binary_prefix;
    string cmakex_deps_clone_prefix;
    string cmakex_deps_install_prefix;
    string cmakex_executor_dir;
    string cmakex_tmp_dir;
    string cmakex_log_dir;
    bool strict_clone = true;  // accept only the clone at the commit exactly as specified
};

void badpars_exit(string_par msg);

// source descriptor is either a directory containing CMakeLists.txt or
// path to a *.cmake file. This function finds out which.
source_descriptor_kind_t evaluate_source_descriptor(string_par x, bool allow_invalid = false);
}

#endif