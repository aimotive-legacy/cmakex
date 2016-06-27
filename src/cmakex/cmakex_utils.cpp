#include "cmakex_utils.h"

#include <adasworks/sx/check.h>

#include "filesystem.h"

namespace cmakex {

namespace fs = filesystem;

cmakex_config_t::cmakex_config_t(string_par cmake_binary_dir, string_par cmake_source_dir)
{
    const string bd = cmake_binary_dir.c_str();
    cmakex_dir = bd + "/_cmakex";
    cmakex_deps_binary_prefix = bd + "/../_deps/b";
    cmakex_deps_clone_prefix = bd + "/../_deps/src";
    cmakex_deps_clone_prefix = bd + "/../_deps/o";
    cmakex_executor_dir = cmakex_dir + "/build_script_executor_project";
    cmakex_tmp_dir = cmakex_dir + "/tmp";
    cmakex_log_dir = cmakex_dir + "/log";
    if (!cmake_source_dir.empty()) {
        deps_script_file = cmake_source_dir.str() + "/deps.cmake";
    }
}

void badpars_exit(string_par msg)
{
    fprintf(stderr, "Error, bad parameters: %s.\n", msg.c_str());
    exit(EXIT_FAILURE);
}

bool evaluate_source_dir(string_par x, bool allow_invalid)
{
    if (fs::is_directory(x.c_str())) {
        if (fs::is_regular_file(x.str() + "/CMakeLists.txt"))
            return true;
        else if (allow_invalid)
            return false;
        else
            badpars_exit(stringf(
                "Source path \"%s\" is a directory but contains no 'CMakeLists.txt'.", x.c_str()));
    } else if (allow_invalid)
        return false;
    else
        badpars_exit(stringf("Source path not found: \"%s\".", x.c_str()));

    CHECK(false);  // never here
    return false;
}

string cmakex_config_t::pkg_binary_dir(string_par pkg_name) const
{
    return cmakex_deps_binary_prefix + "/" + pkg_name.c_str();
}
}
