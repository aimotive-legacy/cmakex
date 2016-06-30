#ifndef INSTALLDB_23423084
#define INSTALLDB_23423084

#include <map>

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {

struct installed_pkg_desc_t
{
    struct depends_item_t
    {
        string pkg_name;
    };

    string name;
    string git_url;
    string git_sha;
    string source_dir;
    vector<depends_item_t> depends;  // all dependencies not only immediate/listed
    vector<string> cmake_args;  // all cmake args including global ones
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
        string incompatible_cmake_args;
        installed_pkg_desc_t pkg_desc;
    };

    InstallDB(string_par binary_dir);

    // evaluate whether the current installation of the package satisfies the request. Provide
    // textual description on failure
    request_eval_result_t evaluate_pkg_request(const pkg_request_t& req);
    maybe<installed_pkg_desc_t> try_get_installed_pkg_desc(string_par pkg_name) const;
    void put_installed_pkg_desc(const installed_pkg_desc_t& p);

private:
    string installed_pkg_desc_path(string_par pkg_name) const;
    string dbpath;
};
}

#endif
