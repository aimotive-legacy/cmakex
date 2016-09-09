#ifndef BUILD_23984623
#define BUILD_23984623

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {

// build the referenced package (project)
// it is cloned out, need to be built with options specified in request
struct build_result_t
{
    vector<string> hijack_modules_needed;  // list of Find*.cmake modules to shadow an official
    // CMake find-module. This vector contains the base name: Find<base-name>.cmake
};

build_result_t build(
    string_par binary_dir,
    string_par pkg_name,        // empty for main project
    string_par pkg_source_dir,  // pkg-root relative for pkg, cwd-relative or abs for main project
    const vector<string>& cmake_args,
    config_name_t config,
    const vector<string>& build_targets,
    bool force_config_step,  // config even there are no cmake_args different to what are in the
                             // cache
    const cmakex_cache_t& cmakex_cache,
    vector<string> build_args,  // by value
    const vector<string>& native_tool_args);
}

#endif
