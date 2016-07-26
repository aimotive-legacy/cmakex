#ifndef CMAKEX_UTILS_23094
#define CMAKEX_UTILS_23094

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {

struct cmakex_cache_t
{
    string home_directory;
};

struct cmakex_config_t
{
    // specify main binary/source dirs.
    cmakex_config_t(string_par cmake_binary_dir);

    string main_binary_dir_common() const;  // returns the arg of the ctor

    // returns the common version, or, if per_config_binary_dirs is set
    // and generator is not a multi-config one than returns the per-config
    // binary dir (e.g. <common-bindir>/$<CONFIG>)
    string main_binary_dir_of_config(string_par config, string_par cmake_generator) const;

    string pkg_binary_dir_common(string_par pkg_name) const;

    // empty config translated to NoConfig
    string pkg_binary_dir_of_config(string_par pkg_name,
                                    string_par config,
                                    string_par cmake_generator) const;
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
    string cmakex_cache_path() const;

    // if cmake_generator is empty then it's assumed that it's the default one
    bool needs_per_config_binary_dirs(string_par cmake_generator) const;

    const cmakex_cache_t& cmakex_cache() const { return cmakex_cache_; }
    bool cmakex_cache_loaded() const { return cmakex_cache_loaded_; }
private:
    const string cmake_binary_dir;
    bool per_config_binary_dirs_ = false;  // in case of single-config generators
    cmakex_cache_t cmakex_cache_;
    bool cmakex_cache_loaded_ = false;
};

void badpars_exit(string_par msg);

// source dir is a directory containing CMakeLists.txt
bool evaluate_source_dir(string_par x, bool allow_invalid = false);
struct configuration_helper_t
{
    // empty config translated to NoConfig
    configuration_helper_t(const cmakex_config_t& cfg,
                           string_par pkg_name,
                           const vector<string>& cmake_args,
                           string_par config);
    string pkg_bin_dir;
    bool multiconfig_generator;
};
string pkg_for_log(string_par pkg_name);

// return same of NoConfig if empty
string same_or_NoConfig(string_par config);
}

#endif