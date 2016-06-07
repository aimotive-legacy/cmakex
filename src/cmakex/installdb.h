#ifndef INSTALLDB_23423084
#define INSTALLDB_23423084

#include "using-decls.h"

#include <map>

namespace cmakex {

struct pkg_clone_pars_t
{
    string git_url;
    string git_tag;
    bool git_tag_is_sha = false;  // false means we don't know
    bool full_clone = false;      // if false, clone only the requested branch at depth=1
};

// pkg request is what comes from the registry (to be implemented)
// and from the local package definition script (the ExternalProject-like
// parameters)
struct pkg_request_t
{
    string name;
    pkg_clone_pars_t clone_pars;
    string source_dir;
    vector<string> depends;
    vector<string> cmake_args;
    vector<string> configs;
};

struct installed_pkg_desc_t
{
    struct depends_item_t
    {
        string pkg_name;
        string source;  // source of the installation: empty = local install
    };

    string name;
    string git_url;
    string git_sha;
    string source_dir;
    vector<depends_item_t> depends;
    vector<string> cmake_args;
    vector<string> configs;
};

struct installed_pkg_files_t
{
    struct file_item_t
    {
        string path;  // relative to install prefix
        string sha;
    };
    using files_t = vector<file_item_t>;
    using files_of_configs_t = std::map<string, files_t>;

    files_of_configs_t files_of_configs;
};
// stores, adds and removes and queries the list of packages and corresponding files
// installed into a directory
class InstallDB
{
public:
    enum request_eval_result_status_t
    {
        invalid_status,
        pkg_request_satisfied,        // the request is satisfied by what is installed
        pkg_request_missing_configs,  // the request is partly satisfied, there are missing configs
        pkg_request_not_installed,    // the requested package is not installed at all
        pkg_request_not_compatible    // the request package is installed with incompatible build
                                      // options
    };
    struct request_eval_result_t
    {
        request_eval_result_status_t status = invalid_status;
        vector<string> missing_configs;
    };

    InstallDB(string_par binary_dir);

    // evaluate whether the current installation of the package satisfies the request. Provide
    // textual description on failure
    tuple<request_eval_result_t, string> evaluate_pkg_request(const pkg_request_t& req);
    maybe<installed_pkg_desc_t> try_get_installed_pkg_desc(string_par pkg_name) const;
    void put_installed_pkg_desc(const installed_pkg_desc_t& p);

private:
    string installed_pkg_desc_path(string_par pkg_name) const;
    string dbpath;
};
}

#endif
