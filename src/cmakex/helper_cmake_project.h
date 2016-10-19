#ifndef HELPER_CMAKE_PROJECT_92374039247
#define HELPER_CMAKE_PROJECT_92374039247

#include "cmakex_utils.h"
#include "using-decls.h"

namespace cmakex {

class HelperCmakeProject
{
public:
    HelperCmakeProject(string_par binary_dir);
    // applies deps_accum_cmake_args if initial config, otherwise the applies the (incremental)
    // command_line_cmake_args
    void configure(const vector<string>& command_line_cmake_args, string_par pkg_name);
    vector<string> run_deps_script(string_par deps_script_file,
                                   bool clear_downloaded_include_files,
                                   string_par pkg_name);

    cmake_cache_t cmake_cache;  // read after configuration

private:
    const string binary_dir;
    const cmakex_config_t cfg;
    const string build_script_executor_binary_dir;
    const string build_script_add_pkg_out_file;
    const string build_script_cmakex_out_file;
};
}

#endif
