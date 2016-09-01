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
    string main_binary_dir_of_config(const config_name_t& config, bool per_config_bin_dirs) const;

    string pkg_binary_dir_common(string_par pkg_name) const;

    // same as main_binary_dir_of_config for packages
    string pkg_binary_dir_of_config(string_par pkg_name,
                                    const config_name_t& config,
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
    // this is the user setting, effective only for single-config generators
    // i.e. it can be true for multi-config but then it's ignored
    bool per_config_bin_dirs() const { return per_config_bin_dirs_; }
private:
    const string cmake_binary_dir;
    bool per_config_bin_dirs_ = false;  // the user setting
    cmakex_cache_t cmakex_cache_;
};

void badpars_exit(string_par msg);

// source dir is a directory containing CMakeLists.txt
bool evaluate_source_dir(string_par x, bool allow_invalid = false);

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

// expects merged arguments: -DA=B instead of -D A=B
parsed_cmake_arg_t parse_cmake_arg(string_par x);
string format_cmake_arg(const parsed_cmake_arg_t& a);

pkg_request_t pkg_request_from_arg_str(const string& pkg_arg_str,
                                       const vector<config_name_t>& default_configs);
pkg_request_t pkg_request_from_args(const vector<string>& pkg_args,
                                    const vector<config_name_t>& default_configs);

// merges arguments: -D A=B -> -DA=B
// throws on invalid arguments
// transforms some variables to switches (-G, -T, -A)
// merges -D and -U switches referring the same variables
// sorts
vector<string> normalize_cmake_args(const vector<string>& x);
void write_cmakex_cache_if_dirty(string_par bin_dir, const cmakex_cache_t& cmakex_cache);

#if 0
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
#endif

struct cmake_cache_tracker_t
{
    void add_pending(const vector<string>& cmake_args);
    void confirm_pending();

    vector<string> pending_cmake_args, cached_cmake_args;
    string c_sha;                     // SHA of the file specified with -C
    string cmake_toolchain_file_sha;  // SHA of the file specified with -DCMAKE_TOOLCHAIN_FILE
};

#if 0
// Tracks the interesting variables of the cmake-cache of a cmake project in a separate file
class CMakeCacheTracker
{
public:
    struct report_t
    {
        maybe<string> cmake_prefix_path;
    };

    static void remove(string_par x);

    // construct before calling a cmake-configure for a cmake project
    CMakeCacheTracker(string_par bin_dir);
    //    CMakeCacheTracker(string_par bin_dir, string_par filename);

    // Call before cmake-configure
    // returns cmake_args and possibly additional settings to bring the cache into the desired state
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

    report_t cmake_config_ok();  // call after successful cmake-config

private:
    string path;
};
#endif

void remove_cmake_cache_tracker(string_par bin_dir);
cmake_cache_tracker_t load_cmake_cache_tracker(string_par bin_dir);
void save_cmake_cache_tracker(string_par bin_dir, const cmake_cache_tracker_t& x);

#if 0
void update_reference_cmake_cache_tracker(string_par pkg_bin_dir_common,
                                          const vector<string>& cmake_args);
#endif
// if there's a CMAKE_PREFIX_PATH, prepends, bool is true
// if there's no, does nothing, bool is false
tuple<vector<string>, bool> cmake_args_prepend_cmake_prefix_path(vector<string> cmake_args,
                                                                 string_par dir);
vector<string> cmakex_prefix_path_to_vector(string_par x);
cmake_cache_t read_cmake_cache(string_par path);
void write_hijack_module(string_par pkg_name, string_par binary_dir);
}

#endif
