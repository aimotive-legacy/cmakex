#ifndef CMAKEX_UTILS_23094
#define CMAKEX_UTILS_23094

#include "using-decls.h"

namespace cmakex {

struct cmakex_config_t
{
    // specify main binary/source dirs.
    // source dir is optional, certain value will not be available if omitted
    cmakex_config_t(string_par cmake_binary_dir, string_par cmake_source_dir = "");

    // returns main dirs if pkg_name is empty
    string pkg_binary_dir(string_par pkg_name) const;
    string pkg_clone_dir(string_par pkg_name) const;
    // string pkg_deps_script_file(string_par pkg_name) const;

    string cmakex_dir() const;  // cmakex internal directory, within main cmake_binary_dir
    string cmakex_executor_dir() const;
    string cmakex_tmp_dir() const;
    string cmakex_log_dir() const;

private:
    const string cmake_binary_dir;
    const string cmake_source_dir;
};

void badpars_exit(string_par msg);

// source dir is a directory containing CMakeLists.txt
bool evaluate_source_dir(string_par x, bool allow_invalid = false);
}

#endif