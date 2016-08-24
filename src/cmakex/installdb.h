#ifndef INSTALLDB_23423084
#define INSTALLDB_23423084

#include <map>

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {

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
    pkg_request_details_against_installed_t evaluate_pkg_request_build_pars(
        string_par pkg_name,
        const pkg_build_pars_t& bp);
    installed_pkg_configs_t try_get_installed_pkg_all_configs(string_par pkg_name) const;
    // maybe<pkg_files_t> try_get_installed_pkg_files(string_par pkg_name) const;

    // if incremental, the desc must be compatible with the currently installed desc
    void install_with_unspecified_files(installed_config_desc_t desc);  // taken by value
    void uninstall_config_if_installed(string_par pkg_name, const config_name_t& config);

private:
    void put_installed_pkg_desc(installed_config_desc_t p);  // taken by value
    // void put_installed_pkg_files(string_par pkg_name, const pkg_files_t& p);

    vector<string> glob_installed_pkg_config_descs(string_par pkg_name) const;

    string installed_pkg_desc_dir(string_par pkg_name) const;
    string installed_pkg_config_desc_path(string_par pkg_name, const config_name_t& config) const;
    // string installed_pkg_files_path(string_par pkg_name) const;

    const string binary_dir;
    const string dbpath;
};
}

#endif
