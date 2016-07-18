#ifndef INSTALLDB_23423084
#define INSTALLDB_23423084

#include <map>

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {

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

// returns a possibly empty list of incompatible cmake args. Only critical cmake args will be
// considered: "-C", "-D", "-G", "-T", "-A"
vector<string> incompatible_cmake_args(const vector<string>& x, const vector<string>& y);

// stores, adds and removes and queries the list of packages and corresponding files
// installed into a directory
class InstallDB
{
public:
    InstallDB(string_par binary_dir);

    // evaluate whether the current installation of the package satisfies the request. Provide
    // textual description on failure
    pkg_request_eval_details_t evaluate_pkg_request(const pkg_desc_t& req);
    maybe<pkg_desc_t> try_get_installed_pkg_desc(string_par pkg_name) const;
    void put_installed_pkg_desc(const pkg_desc_t& p);

private:
    string installed_pkg_desc_path(string_par pkg_name) const;
    string dbpath;
};
}

#endif
