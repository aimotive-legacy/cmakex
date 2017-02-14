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

    // common install dir for dependencies
    string deps_install_dir() const;
    string find_module_hijack_dir() const;

    string cmakex_dir() const;  // cmakex internal directory, within main cmake_binary_dir
    string cmakex_executor_dir() const;
    string cmakex_tmp_dir() const;
    string cmakex_log_dir() const;
    string cmakex_cache_path() const;

    const cmakex_cache_t& cmakex_cache() const { return cmakex_cache_; }
    // this is the user setting, effective only for single-config generators
    // i.e. it can be true for multi-config but then it's ignored
    bool per_config_bin_dirs() const { return per_config_bin_dirs_; }
    string default_deps_source_dir() const;
    string default_deps_build_dir() const;
    string default_deps_install_dir() const;

private:
    const string cmake_binary_dir;
    bool per_config_bin_dirs_ = true;  // the user setting
    cmakex_cache_t cmakex_cache_;
};

AW_NORETURN void badpars_exit(string_par msg);

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

struct cmake_cache_tracker_t
{
    void add_pending(const vector<string>& cmake_args);
    void confirm_pending();

    vector<string> pending_cmake_args, cached_cmake_args;
    string c_sha;                     // SHA of the file specified with -C
    string cmake_toolchain_file_sha;  // SHA of the file specified with -DCMAKE_TOOLCHAIN_FILE
};

void remove_cmake_cache_tracker(string_par bin_dir);
cmake_cache_tracker_t load_cmake_cache_tracker(string_par bin_dir);
void save_cmake_cache_tracker(string_par bin_dir, const cmake_cache_tracker_t& x);

// if cmake_args sets 'var_name' (like -D<var-name>=) then the path will be prepended with 'dir' and
// <new-path>, true returned
// otherwise <unchanged-cmake_args, false> is returned
tuple<vector<string>, bool> cmake_args_prepend_existing_cmake_path_variable(
    vector<string> cmake_args,
    string_par var_name,
    string_par dir);
vector<string> cmakex_prefix_path_to_vector(string_par x, bool env_var);
cmake_cache_t read_cmake_cache(string_par path);
void write_hijack_module(string_par pkg_name, string_par binary_dir);
const string* find_specific_cmake_arg_or_null(string_par cmake_var_name,
                                              const vector<string>& cmake_args);
// checks the CMakeCache.txt of bin_dir and also the input cmake_args
// returns the modified cmake_args which makes sure that by applying it on the cmake config-step
// command line the cmake cache will contain the variable with the requested path
// returns the unchanged cmake_args if no changes are neccessary
vector<string> make_sure_cmake_path_var_contains_path(
    string_par bin_dir,
    string_par var_name,     // like "CMAKE_PREFIX_PATH"
    string_par path_to_add,  // like install dir of the dependencies
    vector<string> cmake_args);

// for cmake scripts
string escape_cmake_arg(string_par x);
string escape_command_line_arg(string_par x);

bool eval_cmake_boolean_or_fail(string_par x);
}

#endif
