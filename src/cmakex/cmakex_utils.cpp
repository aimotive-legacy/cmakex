#include "cmakex_utils.h"

#include <adasworks/sx/check.h>

#include "filesystem.h"
#include "misc_utils.h"

namespace cmakex {

namespace fs = filesystem;

cmakex_config_t::cmakex_config_t(string_par cmake_binary_dir)
    : cmake_binary_dir(cmake_binary_dir.c_str())
{
    // cmakex_deps_binary_prefix = bd + "/../_deps/b";
    // cmakex_deps_clone_prefix = bd + "/../_deps/src";
    // cmakex_deps_clone_prefix = bd + "/../_deps/o";
    // cmakex_executor_dir = cmakex_dir + "/build_script_executor_project";
    // if (!cmake_source_dir.empty()) {
    //    deps_script_file = cmake_source_dir.str() + "/deps.cmake";
    // }
}

string cmakex_config_t::cmakex_dir() const
{
    return cmake_binary_dir + "/_cmakex";
}

string cmakex_config_t::cmakex_executor_dir() const
{
    return cmakex_dir() + "/deps_script_executor_project";
}

string cmakex_config_t::cmakex_tmp_dir() const
{
    return cmakex_dir() + "/tmp";
}

string cmakex_config_t::cmakex_log_dir() const
{
    return cmakex_dir() + "/log";
}

string cmakex_config_t::pkg_clone_dir(string_par pkg_name) const
{
    return cmake_binary_dir + "/_deps/" + pkg_name.c_str();
}

string cmakex_config_t::pkg_binary_dir(string_par pkg_name,
                                       string_par config,
                                       string_par cmake_generator) const
{
    auto r = cmake_binary_dir + "/_deps/" + pkg_name.c_str() + "-build";
    if (per_config_binary_dirs(cmake_generator))
        r += string("-") + config.c_str();
    return r;
}

bool cmakex_config_t::per_config_binary_dirs(string_par cmake_generator) const
{
    if (!per_config_binary_dirs_)
        return false;
    if (cmake_generator == "Xcode" || cmake_generator == "Green Hills MULTI" ||
        starts_with(cmake_generator, "Visual Studio"))
        return false;
#ifdef _WIN32
    if (cmake_generator.empty())  // the default is Visual Studio on windows
        return false;
#endif
    return true;
}

string cmakex_config_t::pkg_install_dir(string_par pkg_name) const
{
    return cmake_binary_dir + "/_deps/" + pkg_name.c_str() + "-install";
}

string cmakex_config_t::deps_install_dir() const
{
    return cmake_binary_dir + "/_deps-install";
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

string pkg_bin_dir_helper(const cmakex_config_t& cfg, const pkg_desc_t& request, string_par config)
{
    string cmake_generator;
    for (auto& c : request.b.cmake_args) {
        if (starts_with(c, "-G")) {
            cmake_generator = make_string(butleft(c, 2));  // 2 is length of "-G"
            break;
        }
    }
    return cfg.pkg_binary_dir(request.name, config, cmake_generator);
}
}
