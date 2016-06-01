#include "cmakex_utils.h"

#include <adasworks/sx/check.h>

#include "filesystem.h"

namespace cmakex {

namespace fs = filesystem;

cmakex_config_t::cmakex_config_t(string_par cmake_binary_dir)
{
    const string bd = cmake_binary_dir.c_str();
    cmakex_dir = bd + "/_cmakex";
    cmakex_deps_binary_prefix = bd + "/../_deps/b";
    cmakex_deps_clone_prefix = bd + "/../_deps/src";
    cmakex_deps_clone_prefix = bd + "/../_deps/o";
    cmakex_executor_dir = cmakex_dir + "/build_script_executor_project";
    cmakex_tmp_dir = cmakex_dir + "/tmp";
    cmakex_log_dir = cmakex_dir + "/log";
}

void badpars_exit(string_par msg)
{
    fprintf(stderr, "Error, bad parameters: %s.\n", msg.c_str());
    exit(EXIT_FAILURE);
}

source_descriptor_kind_t evaluate_source_descriptor(string_par x, bool allow_invalid)
{
    if (fs::is_regular_file(x.c_str())) {
        if (fs::path(x.c_str()).extension().string() == ".cmake")
            return source_descriptor_build_script;
        else if (allow_invalid)
            return source_descriptor_invalid;
        else
            badpars_exit(stringf("Source path is a file but its extension is not '.cmake': \"%s\"",
                                 x.c_str()));
    } else if (fs::is_directory(x.c_str())) {
        if (fs::is_regular_file(x.str() + "/CMakeLists.txt"))
            return source_descriptor_cmakelists_dir;
        else if (allow_invalid)
            return source_descriptor_invalid;
        else
            badpars_exit(stringf(
                "Source path \"%s\" is a directory but contains no 'CMakeLists.txt'.", x.c_str()));
    } else if (allow_invalid)
        return source_descriptor_invalid;
    else
        badpars_exit(stringf("Source path not found: \"%s\".", x.c_str()));

    CHECK(false);  // never here
    return source_descriptor_invalid;
}
}