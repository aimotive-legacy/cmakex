#include "cmakex_utils.h"

#include <cereal/archives/json.hpp>
#include <nowide/fstream.hpp>

#include <adasworks/sx/check.h>

#include "filesystem.h"
#include "misc_utils.h"

CEREAL_CLASS_VERSION(cmakex::cmakex_cache_t, 1)

namespace cmakex {

namespace fs = filesystem;

#define A(X) cereal::make_nvp(#X, m.X)

template <class Archive>
void serialize(Archive& archive, cmakex_cache_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(home_directory));
}
#undef A

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
    string path = cmakex_cache_path();
    if (fs::is_regular_file(path)) {
        nowide::ifstream f(path);
        if (!f.good())
            throwf("Can't open existing file \"%s\" for reading.", path.c_str());
        string what;
        try {
            // otherwise it must succeed
            cereal::JSONInputArchive a(f);
            a(cmakex_cache_);
        } catch (const exception& e) {
            what = e.what();
        } catch (...) {
            what = "unknown exception.";
        }
        throwf("Can't read \"%s\", reason: %s", path.c_str(), what.c_str());
    }
}

string cmakex_config_t::cmakex_cache_path() const
{
    return cmake_binary_dir + "/" + k_cmakex_cache_filename;
}

string cmakex_config_t::main_binary_dir_common() const
{
    return cmake_binary_dir;
}

string cmakex_config_t::main_binary_dir_of_config(string_par config,
                                                  string_par cmake_generator) const
{
    string r = cmake_binary_dir;
    if (!config.empty() && needs_per_config_binary_dirs(cmake_generator))
        r += string("/") + same_or_NoConfig(config);
    return r;
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

string cmakex_config_t::pkg_binary_dir_common(string_par pkg_name) const
{
    return cmake_binary_dir + "/_deps/" + pkg_name.c_str() + "-build";
}

string cmakex_config_t::pkg_binary_dir_of_config(string_par pkg_name,
                                                 string_par config,
                                                 string_par cmake_generator) const
{
    auto r = pkg_binary_dir_common(pkg_name);
    if (!config.empty() && needs_per_config_binary_dirs(cmake_generator))
        r += string("/") + same_or_NoConfig(config);
    return r;
}

bool is_generator_multiconfig(string_par cmake_generator)
{
    if (cmake_generator == "Xcode" || cmake_generator == "Green Hills MULTI" ||
        starts_with(cmake_generator, "Visual Studio"))
        return true;
#ifdef _WIN32
    if (cmake_generator.empty())  // the default is Visual Studio on windows
        return true;
#endif
    return false;
}

bool cmakex_config_t::needs_per_config_binary_dirs(string_par cmake_generator) const
{
    if (!per_config_binary_dirs_)
        return false;
    return !is_generator_multiconfig(cmake_generator);
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

configuration_helper_t::configuration_helper_t(const cmakex_config_t& cfg,
                                               string_par pkg_name,
                                               const vector<string>& cmake_args,
                                               string_par config)
{
    string cmake_generator;
    for (auto& c : cmake_args) {
        if (starts_with(c, "-G")) {
            cmake_generator = make_string(butleft(c, 2));  // 2 is length of "-G"
            break;
        }
    }
    pkg_bin_dir = cfg.pkg_binary_dir_of_config(pkg_name, config, cmake_generator);
    multiconfig_generator = is_generator_multiconfig(cmake_generator);
}

string pkg_for_log(string_par pkg_name)
{
    return stringf("[%s]", pkg_name.c_str());
}
string same_or_NoConfig(string_par config)
{
    return config.empty() ? "NoConfig" : config.str();
}
}
