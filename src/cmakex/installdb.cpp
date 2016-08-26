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
CEREAL_CLASS_VERSION(cmakex::installed_config_desc_t, 1)

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
void serialize(Archive& archive, pkg_build_pars_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(source_dir), A(cmake_args), A(configs));
}

template <class Archive>
void serialize(Archive& archive, pkg_clone_pars_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(git_url), A(git_tag));
}

template <class Archive>
void serialize(Archive& archive, pkg_clone_pars_sha_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(git_url), A(git_sha));
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
void serialize(Archive& archive, installed_config_desc_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(pkg_name), A(config), A(c), A(b.source_dir), A(b.cmake_args), A(deps_shas));
}

#undef A

string calc_sha(const installed_config_desc_t& x)
{
    std::ostringstream oss;
    cereal::PortableBinaryOutputArchive a(oss);
    a(x);
    return string_sha(oss.str());
}

InstallDB::InstallDB(string_par binary_dir)
    : binary_dir(binary_dir.str()),
      dbpath(cmakex_config_t(binary_dir).cmakex_dir() + "/" + "installdb")
{
    if (!fs::exists(dbpath))
        fs::create_directories(dbpath);  // must be able to create the path
}

string config_from_installed_pkg_config_path(string_par s)
{
    return fs::path(s).stem().string();
}

installed_pkg_configs_t InstallDB::try_get_installed_pkg_all_configs(string_par pkg_name) const
{
    installed_pkg_configs_t r;
    auto paths = glob_installed_pkg_config_descs(pkg_name);
    for (auto& p : paths) {
        CHECK(fs::is_regular_file(p));  // just globbed
        installed_config_desc_t y = installed_config_desc_t::uninitialized_installed_config_desc();
        load_json_input_archive(p, y);
        if (y.pkg_name != pkg_name)
            throwf(
                "Invalid installed-config file: the pkg_name read from \"%s\" should be '%s' but "
                "it is '%s'",
                p.c_str(), y.pkg_name.c_str(), pkg_name.c_str());
        auto path_from_deserialized = installed_pkg_config_desc_path(pkg_name, y.config);
        if (!fs::equivalent(path_from_deserialized, p))
            throwf("Invalid installed-config file: the configuration type read from \"%s\" is '%s'",
                   p.c_str(), y.config.get_prefer_NoConfig().c_str());

        auto new_result_value_it_bool = r.config_descs.emplace(y.config, y);
        if (new_result_value_it_bool.second)
            throwf(
                "Installed-configuration file \"%s\" contains configuration '%s' but that config "
                "has already been listed in another installed-configuration file of the same "
                "package.",
                p.c_str(), y.config.get_prefer_NoConfig().c_str());
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
    p.b.cmake_args = normalize_cmake_args(p.b.cmake_args);
    auto path = installed_pkg_config_desc_path(p.pkg_name, p.config);
    save_json_output_archive(path, p);
}

/*void InstallDB::put_installed_pkg_files(string_par pkg_name, const pkg_files_t& p)
{
    auto path = installed_pkg_files_path(pkg_name);
    save_json_output_archive(path, p);
}
*/

vector<string> InstallDB::glob_installed_pkg_config_descs(string_par pkg_name) const
{
    vector<string> r;
    auto dir = installed_pkg_desc_dir(pkg_name);
    if (fs::is_directory(dir)) {
        for (Poco::DirectoryIterator it(dir); it != Poco::DirectoryIterator(); ++it) {
            if (it->isFile())
                r.emplace_back(it->path());
        }
    }
    return r;
}

string InstallDB::installed_pkg_desc_dir(string_par pkg_name) const
{
    return stringf("%s/%s", dbpath.c_str(), pkg_name.c_str());
}

string InstallDB::installed_pkg_config_desc_path(string_par pkg_name,
                                                 const config_name_t& config) const
{
    return stringf("%s/%s.json", installed_pkg_desc_dir(pkg_name).c_str(),
                   config.get_lowercase_prefer_noconfig().c_str());
}

/*string InstallDB::installed_pkg_files_path(string_par pkg_name) const
{
    return dbpath + "/" + pkg_name.c_str() + "-files.json";
}
*/
bool is_critical_cmake_arg(const string& s)
{
    for (auto p : {"-C", "-D", "-G", "-T", "-A"}) {
        if (starts_with(s, p))
            return true;
    }
    return false;
}

vector<string> incompatible_cmake_args(const vector<string>& x, const vector<string>& y)
{
    vector<string> r;
    auto cx = normalize_cmake_args(x);
    auto cy = normalize_cmake_args(y);
    for (auto& o : set_difference(cx, cy))
        if (is_critical_cmake_arg(o))
            r.emplace_back(o);
    for (auto& o : set_difference(cy, cx))
        if (is_critical_cmake_arg(o))
            r.emplace_back(o);
    return r;
}

pkg_request_details_against_installed_t InstallDB::evaluate_pkg_request_build_pars(
    string_par pkg_name,
    const pkg_build_pars_t& bp)
{
    auto installed_configs = try_get_installed_pkg_all_configs(pkg_name);
    pkg_request_details_against_installed_t r;
    for (const auto& req_config : bp.configs) {
        auto it_installed_config = installed_configs.config_descs.find(req_config);
        if (it_installed_config == installed_configs.config_descs.end()) {
            r.emplace(req_config, installed_config_desc_t(pkg_name, req_config));
            r.at(req_config).status = pkg_request_not_installed;
        } else {
            auto& cd = it_installed_config->second;
            r.emplace(req_config, cd);
            auto ica = incompatible_cmake_args(cd.b.cmake_args, bp.cmake_args);
            if (bp.source_dir != cd.b.source_dir) {
                ica.emplace_back(stringf("(different source dirs: '%s' and '%s')",
                                         cd.b.source_dir.c_str(), bp.source_dir.c_str()));
            }
            if (ica.empty()) {
                r.at(req_config).status = pkg_request_satisfied;
            } else {
                r.at(req_config).status = pkg_request_not_compatible;
                r.at(req_config).incompatible_cmake_args = join(ica, " ");
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
        log_error("Failed to remove \"%s\", reason: %s", f.c_str(), e.what());
    } catch (...) {
        log_error("Failed to remove \"%s\", reason is unknown.", f.c_str());
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
}
