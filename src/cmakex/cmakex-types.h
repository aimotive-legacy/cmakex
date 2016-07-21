#ifndef CMAKEX_TYPES_29034023
#define CMAKEX_TYPES_29034023

#include <map>

#include "using-decls.h"

namespace cmakex {

static const char* k_deps_script_filename = "deps.cmake";
static const char* k_log_extension = ".log";

enum git_tag_kind_t
{
    // order is important
    git_tag_not_checked,
    git_tag_is_not_sha,    // it's not sha-like
    git_tag_could_be_sha,  // it's sha-like but not checked
    git_tag_must_be_sha,   // it's sha-like and server returned not found
    git_tag_is_sha,        // we initialized it with a known sha
};

bool sha_like(string_par x);

inline git_tag_kind_t initial_git_tag_kind(string git_tag)
{
    return sha_like(git_tag) ? git_tag_could_be_sha : git_tag_is_not_sha;
}

struct pkg_clone_pars_t
{
    string git_url;
    string git_tag;
};

struct pkg_build_pars_t
{
    string source_dir;  // (relative) directory containing CMakeLists.txt
    // cmake_args:
    // - never contains source and binary dir flags or paths
    // - contains CMAKE_INSTALL_PREFIX, CMAKE_PREFIX_PATH, CMAKE_MODULE_PATH only when describes
    //   command line
    // - contains global args when used as package request / installed package description
    vector<string> cmake_args;  // all cmake args including global ones
    vector<string> configs;     // Debug, Release, etc..
};

struct pkg_desc_t
{
    string name;
    pkg_clone_pars_t c;
    pkg_build_pars_t b;
    vector<string> depends;  // all dependencies not only immediate/listed
    string sha_of_installed_files;
    std::map<string, string> dep_shas;  // sha's of dependencies at the time of the build
};

struct pkg_files_t
{
    struct file_item_t
    {
        string path;  // relative to install prefix
        string sha;
    };
    vector<file_item_t> files;
};

struct pkg_desc_with_files_t : pkg_desc_t, pkg_files_t
{
};

struct pkg_request_t : pkg_desc_t
{
    bool git_shallow = true;  // if false, clone only the requested branch at depth=1
};

struct cmakex_pars_t : public pkg_request_t
{
    enum subcommand_t
    {
        subcommand_invalid,
        subcommand_cmake_steps
    } subcommand = subcommand_invalid;

    bool flag_c = false;
    bool flag_b = false;
    bool flag_t = false;
    bool binary_dir_valid = false;
    string binary_dir;
    vector<string> build_args;
    vector<string> native_tool_args;
    vector<string> build_targets;
    bool config_args_besides_binary_dir = false;
    vector<string> add_pkgs;
    bool deps = false;
    bool strict_commits = false;
};

enum pkg_request_eval_against_installed_t
{
    invalid_status,
    pkg_request_satisfied,        // the request is satisfied by what is installed
    pkg_request_missing_configs,  // the request is partly satisfied, there are missing configs
    pkg_request_not_installed,    // the requested package is not installed at all
    pkg_request_not_compatible    // the request package is installed with incompatible build
                                  // options
};

struct pkg_request_eval_details_t
{
    pkg_request_eval_against_installed_t status = invalid_status;
    vector<string> missing_configs;
    string incompatible_cmake_args;
    pkg_desc_t pkg_desc;
};
}

#endif
