#ifndef CMAKEX_UTILS_23094
#define CMAKEX_UTILS_23094

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {

struct cmakex_config_t
{
    // specify main binary/source dirs.
    cmakex_config_t(string_par cmake_binary_dir);

    string main_binary_dir_common() const;  // returns the arg of the ctor

    // returns cmake_binary_dir, or, if per_config_binary_dirs is true
    // and config is not empty, than returns the per-config binary dir (e.g.
    // <common-bindir>/$<CONFIG>)
    string main_binary_dir_of_config(string_par config, bool per_config_bin_dirs) const;

    string pkg_binary_dir_common(string_par pkg_name) const;

    // same as main_binary_dir_of_config for packages
    string pkg_binary_dir_of_config(string_par pkg_name,
                                    string_par config,
                                    bool per_config_bin_dirs) const;

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

    const cmakex_cache_t& cmakex_cache() const { return cmakex_cache_; }
    bool cmakex_cache_loaded() const { return cmakex_cache_loaded_; }
    // this is the user setting, effective only for single-config generators
    // i.e. it can be true for multi-config but then it's ignored
    bool per_config_bin_dirs() const { return per_config_bin_dirs_; }
private:
    const string cmake_binary_dir;
    bool per_config_bin_dirs_ = false;  // the user setting
    cmakex_cache_t cmakex_cache_;
    bool cmakex_cache_loaded_ = false;
};

void badpars_exit(string_par msg);

// source dir is a directory containing CMakeLists.txt
bool evaluate_source_dir(string_par x, bool allow_invalid = false);

#if 0
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
#endif

string pkg_for_log(string_par pkg_name);

// return same of NoConfig if empty
string same_or_NoConfig(string_par config);

// if cmake_generator is empty then uses platform-defaults
bool is_generator_multiconfig(string_par cmake_generator);

string extract_generator_from_cmake_args(const vector<string>& cmake_args);

struct parsed_cmake_arg_t
{
    string switch_;  // like "-D" or "--trace" or "--graphwiz"
    string name;   // for -D or -U: part after the switch, before ':' or '=' or until end of string
    string type;   // for -D: part between ':' and '='
    string value;  // part after '=' or, for -C, -G, -T, A: the argument
};

// expects merges arguments: -DA=B instead of -D A=B
parsed_cmake_arg_t parse_cmake_arg(string_par x);

pkg_request_t pkg_request_from_arg_str(const string& pkg_arg_str);
pkg_request_t pkg_request_from_args(const vector<string>& pkg_args);

// merges arguments: -D A=B -> -DA=B
// throws on invalid arguments
// transforms some variables to switches (-G, -T, -A)
// merges -D and -U switches referring the same variables
// sorts
vector<string> normalize_cmake_args(const vector<string>& x);
void write_cmakex_cache_if_dirty(string_par bin_dir, const cmakex_cache_t& cmakex_cache);

// json file used by CMakeCacheTracker
struct cmake_cache_tracker_t
{
    enum var_status_t
    {
        // the actual values defined to an intuitive number for json
        vs_to_be_removed = -1,
        vs_in_cmakecache = 0,
        vs_to_be_defined = 1
    };
    struct var_t
    {
        string value;  // the full CMAKE_ARG
        var_status_t status;
    };

    // both vars are variable name - variable value pairs
    // in case if -C, -G, -T, -A the variable name is the switch
    std::map<string, var_t> vars;

    string c_sha;                     // SHA of the file specified with -C
    string cmake_toolchain_file_sha;  // SHA of the file specified with -DCMAKE_TOOLCHAIN_FILE
};

// Tracks the interesting variables of the cmake-cache of a cmake project in a separate file
class CMakeCacheTracker
{
public:
    // construct before calling a cmake-configure for a cmake project
    CMakeCacheTracker(string_par bin_dir);

    // Call before cmake-configure
    // returns cmake_args and possibly dditional settings to bring the cache into the desired state
    // (this depends on how the desired_vars and assumed_vars compare)
    // If force_input_cmake_args then all the input cmake_args will be returned, too
    // If it's false, only those will be returned whose cache values is either different or
    // uncertain
    // The function always returns those cmake args not in the request but whose cache values is
    // uncertain.
    // If reference target is given, then other cmake args will be added in order to
    // take this tracker into the reference state.
    vector<string> about_to_configure(const vector<string>& cmake_args,
                                      bool force_input_cmake_args,
                                      string_par reference_target_path = "");

    void cmake_config_ok();  // call after successful cmake-config

private:
    string path;
};

vector<string> cmake_args_prepend_cmake_prefix_path(vector<string> cmake_args, string_par dir);
}

#endif