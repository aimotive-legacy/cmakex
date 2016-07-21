#ifndef CMAKEX_UTILS_23094
#define CMAKEX_UTILS_23094

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {

struct cmakex_config_t
{
    // specify main binary/source dirs.
    cmakex_config_t(string_par cmake_binary_dir);

    // returns main dirs if pkg_name is empty
    string pkg_binary_dir(string_par pkg_name, string_par config, string_par cmake_generator) const;
    string pkg_clone_dir(string_par pkg_name) const;
    // string pkg_deps_script_file(string_par pkg_name) const;

    // package-local install dir
    string pkg_install_dir(string_par pkg_name) const;
    // common install dir for dependencies
    string deps_install_dir() const;

    string cmakex_dir() const;  // cmakex internal directory, within main cmake_binary_dir
    string cmakex_executor_dir() const;
    string cmakex_tmp_dir() const;
    string cmakex_log_dir() const;

    // if cmake_generator is empty then it's assumed that it's the default one
    bool per_config_binary_dirs(string_par cmake_generator) const;

private:
    const string cmake_binary_dir;
    bool per_config_binary_dirs_ = false;  // in case of single-config generators
};

void badpars_exit(string_par msg);

// source dir is a directory containing CMakeLists.txt
bool evaluate_source_dir(string_par x, bool allow_invalid = false);
string pkg_bin_dir_helper(const cmakex_config_t& cfg, const pkg_desc_t& request, string_par config);
}

#endif