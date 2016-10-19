#include "installdb.h"

#include <Poco/DirectoryIterator.h>
#include <Poco/SHA1Engine.h>
#include <adasworks/sx/algorithm.h>

#include "cereal_utils.h"
#include "cmakex_utils.h"
#include "filesystem.h"
#include "misc_utils.h"
#include "print.h"

CEREAL_CLASS_VERSION(cmakex::pkg_desc_t, 1)
CEREAL_CLASS_VERSION(cmakex::pkg_build_pars_t, 1)
CEREAL_CLASS_VERSION(cmakex::pkg_clone_pars_t, 1)
// CEREAL_CLASS_VERSION(cmakex::pkg_files_t, 1)
CEREAL_CLASS_VERSION(cmakex::installed_config_desc_t, 3)

namespace cmakex {

namespace fs = filesystem;

#define A(X) cereal::make_nvp(#X, m.X)

template <class Archive>
void load(Archive& archive, config_name_t& x)
{
    string y;
    archive(y);
    x = config_name_t(y);
}

template <class Archive>
void save(Archive& archive, const config_name_t& x)
{
    archive(x.get_prefer_NoConfig());
}

template <class Archive>
void serialize(Archive& archive, pkg_clone_pars_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(git_url), A(git_tag));
}

template <class Archive>
void serialize(Archive& archive, pkg_desc_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(name), A(c), A(b), A(depends));
}

/*
template <class Archive>
void serialize(Archive& archive, pkg_files_t::file_item_t& m)
{
    archive(A(path), A(sha));
}

template <class Archive>
void serialize(Archive& archive, pkg_files_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(files));
}
*/

template <class Archive>
void serialize(Archive& archive, final_cmake_args_t& m)
{
    archive(A(args), A(c_sha), A(cmake_toolchain_file_sha));
}

template <class Archive>
void serialize(Archive& archive, installed_config_desc_t& m, uint32_t version)
{
    THROW_UNLESS(version == 2 || version == 3);
    archive(A(pkg_name), A(config), A(git_url), A(git_sha), A(source_dir), A(final_cmake_args),
            A(deps_shas));
    if (version == 3)
        archive(A(hijack_modules_needed));
}

#undef A

string calc_sha(const installed_config_desc_t& x)
{
    std::ostringstream oss;
    cereal::PortableBinaryOutputArchive a(oss);
    a(x);
    return string_sha(oss.str());
}

namespace {
string dbpath_from_prefix_path(string_par prefix_path)
{
    return prefix_path.str() + "/_cmakex/installdb";
}
}

InstallDB::InstallDB(string_par binary_dir)
    : binary_dir(binary_dir.str()),
      dbpath(dbpath_from_prefix_path(cmakex_config_t(binary_dir).deps_install_dir()))
{
    if (!fs::exists(dbpath))
        fs::create_directories(dbpath);  // must be able to create the path
}

string config_from_installed_pkg_config_path(string_par s)
{
    return fs::path(s).stem().string();
}

installed_pkg_configs_t InstallDB::try_get_installed_pkg_all_configs(
    string_par pkg_name,
    const vector<string>& prefix_paths) const
{
    auto r = quick_check_on_prefix_paths(pkg_name, prefix_paths);
    return try_get_installed_pkg_all_configs(pkg_name, get<0>(r));
}

installed_pkg_configs_t InstallDB::try_get_installed_pkg_all_configs(string_par pkg_name,
                                                                     string_par prefix_path) const
{
    installed_pkg_configs_t r;
    auto paths = glob_installed_pkg_config_descs(pkg_name, prefix_path);
    LOG_TRACE("glob_installed_pkg_config_descs(%s, %s) -> [%s]", pkg_name.c_str(),
              prefix_path.c_str(), join(paths, ", ").c_str());
    for (auto& p : paths) {
        CHECK(fs::is_regular_file(p));  // just globbed
        installed_config_desc_t y = installed_config_desc_t::uninitialized_installed_config_desc();
        load_json_input_archive(p, y);
        if (y.pkg_name != pkg_name)
            throwf(
                "Invalid installed-config file: the pkg_name read from %s should be '%s' but "
                "it is '%s'",
                path_for_log(p).c_str(), y.pkg_name.c_str(), pkg_name.c_str());
        auto path_from_deserialized = installed_pkg_config_desc_path(pkg_name, y.config);
        if (fs::path(path_from_deserialized).filename().string() !=
            fs::path(p).filename().string()) {
            string v;
            if (g_verbose)
                v = stringf(", reconstructed path: %s",
                            path_for_log(path_from_deserialized).c_str());
            throwf("Invalid installed-config file: the configuration type read from %s is '%s'%s",
                   path_for_log(p).c_str(), y.config.get_prefer_NoConfig().c_str(), v.c_str());
        }

        auto new_result_value_it_bool = r.config_descs.emplace(y.config, y);
        if (!new_result_value_it_bool.second)
            throwf(
                "Installed-configuration file %s contains configuration '%s' but that config "
                "has already been listed in another installed-configuration file of the same "
                "package.",
                path_for_log(p).c_str(), y.config.get_prefer_NoConfig().c_str());
    }
    return r;
}

/*
maybe<pkg_files_t> InstallDB::try_get_installed_pkg_files(string_par pkg_name) const
{
    auto path = installed_pkg_files_path(pkg_name);
    if (!fs::is_regular_file(path))
        return nothing;
    maybe<pkg_files_t> r(in_place);
    load_json_input_archive(path, *r);
    return r;
}
*/

void InstallDB::put_installed_pkg_desc(installed_config_desc_t p)
{
    p.final_cmake_args.args = normalize_cmake_args(p.final_cmake_args.args);
    auto dir = installed_pkg_desc_dir(p.pkg_name, "");
    fs::create_directories(dir);
    auto path = installed_pkg_config_desc_path(p.pkg_name, p.config);
    save_json_output_archive(path, p);
}

/*void InstallDB::put_installed_pkg_files(string_par pkg_name, const pkg_files_t& p)
{
    auto path = installed_pkg_files_path(pkg_name);
    save_json_output_archive(path, p);
}
*/

vector<string> InstallDB::glob_installed_pkg_config_descs(string_par pkg_name,
                                                          string_par prefix_path) const
{
    vector<string> r;
    auto dir = installed_pkg_desc_dir(pkg_name, prefix_path);
    if (fs::is_directory(dir)) {
        for (Poco::DirectoryIterator it(dir); it != Poco::DirectoryIterator(); ++it) {
            if (it->isFile())
                r.emplace_back(it->path());
        }
    }
    return r;
}

string InstallDB::installed_pkg_desc_dir(string_par pkg_name, string_par prefix_path) const
{
    return stringf("%s/%s", prefix_path.empty() ? dbpath.c_str()
                                                : dbpath_from_prefix_path(prefix_path).c_str(),
                   pkg_name.c_str());
}

string InstallDB::installed_pkg_config_desc_path(string_par pkg_name,
                                                 const config_name_t& config) const
{
    return stringf("%s/%s.json", installed_pkg_desc_dir(pkg_name, "").c_str(),
                   tolower(config.get_prefer_NoConfig()).c_str());
}

/*string InstallDB::installed_pkg_files_path(string_par pkg_name) const
{
    return dbpath + "/" + pkg_name.c_str() + "-files.json";
}
*/
enum cmake_arg_criticalness_t
{
    cac_noncritical,                // like --trace
    cac_critical_for_local_builds,  // like -DCMAKE_PREFIX_PATH=... which is ignored for packages
                                    // not built locally
    cac_critical                    // like -DMYVAR=VALUE
};

cmake_arg_criticalness_t is_critical_cmake_arg(const parsed_cmake_arg_t& pca)
{
    if (pca.switch_ == "-C" || pca.switch_ == "-T" || pca.switch_ == "-A")
        return cac_critical;
    if (pca.switch_ == "-D") {
        if (pca.name == "CMAKE_INSTALL_PREFIX" || pca.name == "CMAKE_PREFIX_PATH" ||
            pca.name == "CMAKE_MODULE_PATH")
            return cac_critical_for_local_builds;
        else
            return cac_critical;
    }
    //-G is noncritical, the other variables will make the difference, like compiler_id, etc...
    return cac_noncritical;
}

tuple<vector<string>, vector<string>> incompatible_cmake_args(const final_cmake_args_t& x,
                                                              const final_cmake_args_t& y)
{
    auto r = incompatible_cmake_args(x.args, y.args, false);
    if (x.c_sha != y.c_sha) {
        get<0>(r).emplace_back("(different files for the switch '-C')");
        get<1>(r).emplace_back("(different files for the switch '-C')");
    }
    if (x.cmake_toolchain_file_sha != y.cmake_toolchain_file_sha) {
        get<0>(r).emplace_back("(different CMake toolchain files)");
        get<1>(r).emplace_back("(different CMake toolchain files)");
    }

    return r;
}

tuple<vector<string>, vector<string>> incompatible_cmake_args(const vector<string>& x,
                                                              const vector<string>& y,
                                                              bool consider_c_and_toolchain)
{
    tuple<vector<string>, vector<string>> r;

    auto cx = normalize_cmake_args(x);
    auto cy = normalize_cmake_args(y);

    auto handle_arg = [&r, consider_c_and_toolchain](string_par o) {
        auto pca = parse_cmake_arg(o);
        if (!consider_c_and_toolchain &&
            ((pca.switch_ == "-D" && pca.name == "CMAKE_TOOLCHAIN_FILE") || pca.switch_ == "-C"))
            return;  // handled separately
        switch (is_critical_cmake_arg(pca)) {
            case cac_noncritical:
                break;
            case cac_critical:
                get<0>(r).emplace_back(o.str());
                get<1>(r).emplace_back(o.str());
                break;
            case cac_critical_for_local_builds:
                get<0>(r).emplace_back(o.str());
                break;
            default:
                CHECK(false);
        }
    };

    for (auto& o : set_difference(cx, cy)) {
        handle_arg(o);
    }
    for (auto& o : set_difference(cy, cx)) {
        handle_arg(o);
    }

    return r;
}

pkg_request_details_against_installed_t InstallDB::evaluate_pkg_request_build_pars(
    string_par pkg_name,
    string_par bp_source_dir,
    const std::map<config_name_t, final_cmake_args_t>& bp_final_cmake_args,
    string_par prefix_path)
{
    LOG_TRACE("entering evaluate_pkg_request_build_pars(%s, %s, [%s], %s)",
              pkg_for_log(pkg_name).c_str(), path_for_log(bp_source_dir).c_str(),
              join(get_prefer_NoConfig(keys_of_map(bp_final_cmake_args)), ", ").c_str(),
              prefix_path.c_str());

    auto installed_configs = try_get_installed_pkg_all_configs(pkg_name, prefix_path);

    LOG_TRACE("called try_get_installed_pkg_all_configs(%s, %s) -> configs [%s]",
              pkg_for_log(pkg_name).c_str(), prefix_path.c_str(),
              join(get_prefer_NoConfig(keys_of_map(installed_configs.config_descs)), ", ").c_str());

    pkg_request_details_against_installed_t r;
    for (auto& kv : bp_final_cmake_args) {
        auto& req_config = kv.first;
        auto it_installed_config = installed_configs.config_descs.find(req_config);
        if (it_installed_config == installed_configs.config_descs.end()) {
            r.emplace(req_config, installed_config_desc_t(pkg_name, req_config));
            r.at(req_config).status = pkg_request_not_installed;
        } else {
            auto& cd = it_installed_config->second;
            r.emplace(req_config, cd);
            vector<string> ica_local, ica_any;
            tie(ica_local, ica_any) = incompatible_cmake_args(cd.final_cmake_args, kv.second);
            vector<string> ica_critical;
            if (bp_source_dir != cd.source_dir) {
                auto x = stringf("(different source dirs: '%s' and '%s')", cd.source_dir.c_str(),
                                 bp_source_dir.c_str());
                ica_local.emplace_back(x);
                ica_any.emplace_back(x);
            }
            if (ica_local.empty()) {
                r.at(req_config).status = pkg_request_satisfied;
            } else if (ica_any.empty()) {
                r.at(req_config).status = pkg_request_different_but_satisfied;
                r.at(req_config).incompatible_cmake_args_local = join(ica_local, " ");
            } else {
                r.at(req_config).status = pkg_request_different;
                r.at(req_config).incompatible_cmake_args_local = join(ica_local, " ");
                r.at(req_config).incompatible_cmake_args_any = join(ica_any, " ");
            }
        }
    }
    return r;
}

void enumerate_files_recursively(string_par dir, vector<string>& u)
{
    for (Poco::DirectoryIterator it(dir.str()); it != Poco::DirectoryIterator(); ++it) {
        if (it->isDirectory()) {
            enumerate_files_recursively(it->path(), u);
        } else {
            u.emplace_back(it->path());
        }
    }
}
vector<string> enumerate_files_recursively(string_par dir)
{
    vector<string> u;
    enumerate_files_recursively(dir, u);
    return u;
}

void InstallDB::install_with_unspecified_files(installed_config_desc_t desc)
{
    uninstall_config_if_installed(desc.pkg_name, desc.config);
    put_installed_pkg_desc(desc);
}

namespace {
// remove, catch and log errors
void remove_and_log_error(string_par f)
{
    // todo instead of exceptions we should call the error_code returning remove()
    try {
        fs::remove(f.c_str());
    } catch (const exception& e) {
        log_error("Failed to remove %s, reason: %s", path_for_log(f).c_str(), e.what());
    } catch (...) {
        log_error("Failed to remove %s, reason is unknown.", path_for_log(f).c_str());
    }
}
}

void InstallDB::uninstall_config_if_installed(string_par pkg_name, const config_name_t& config)
{
    // todo uninstall files, too, if they're registered with this installation
    string path = installed_pkg_config_desc_path(pkg_name, config);
    if (fs::exists(path))
        remove_and_log_error(path);
}
tuple<string, vector<config_name_t>> InstallDB::quick_check_on_prefix_paths(
    string_par pkg_name,
    const vector<string>& prefix_paths) const
{
    LOG_TRACE("quick_check_on_prefix_paths: %s [%s]", pkg_for_log(pkg_name).c_str(), join(prefix_paths, ", ").c_str());
    tuple<string, vector<config_name_t>> result;
    vector<string> v;
    v.reserve(prefix_paths.size() + 1);
    if (!glob_installed_pkg_config_descs(pkg_name, "").empty())
        v.emplace_back("");
    for (auto& p : prefix_paths) {
        if (!glob_installed_pkg_config_descs(pkg_name, p).empty())
            v.emplace_back(p);
    }
    if (v.size() == 1) {
        if (!v[0].empty()) {
            get<0>(result) = v[0];
            auto cs = try_get_installed_pkg_all_configs(pkg_name, v[0]);
            for (auto& kv : cs.config_descs)
                get<1>(result).emplace_back(kv.first);
        }
    } else if (v.size() > 1) {
        for (auto& x : v) {
            if (x.empty())
                x = cmakex_config_t(binary_dir).deps_install_dir();
            x = path_for_log(x);
        }
        throwf(
            "The package %s is installed on multiple prefix paths: [%s]. Remove all but one "
            "installation to resolve ambiguity",
            pkg_for_log(pkg_name).c_str(), join(v, ", ").c_str());
    }
    return result;
}
}
