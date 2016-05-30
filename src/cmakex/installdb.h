#ifndef INSTALLDB_23423084
#define INSTALLDB_23423084

#include "using-decls.h"

namespace cmakex {

// pkg request is what comes from the registry (to be implemented)
// and from the local package definition script (the ExternalProject-like
// parameters)
struct pkg_request_t
{
    string name;
    string git_url;
    string git_tag;
    string source_dir;
    vector<string> depends;
    vector<string> cmake_args;
    vector<string> configs;
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
    tuple<request_eval_result_t, string> evaluate_pkg_request(const pkg_request_t& req);
    string installed_commit(string_par pkg_name);
};
}

#endif
