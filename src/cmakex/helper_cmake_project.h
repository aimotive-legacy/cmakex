#ifndef HELPER_CMAKE_PROJECT_92374039247
#define HELPER_CMAKE_PROJECT_92374039247

#include "cmakex_utils.h"
#include "using-decls.h"

namespace cmakex {

class HelperCmakeProject
{
public:
    HelperCmakeProject(string_par binary_dir);
    void configure(const vector<string>& global_cmake_args, const cmakex_cache_t& cmakex_cache);
    vector<string> run_deps_script(string_par deps_script_file);

private:
    const string binary_dir;
    const cmakex_config_t cfg;
    const string build_script_executor_binary_dir;
    const string build_script_add_pkg_out_file;
    const string build_script_cmakex_out_file;
};
}

#endif