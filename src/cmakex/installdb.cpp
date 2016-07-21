#include "installdb.h"

#include <Poco/DirectoryIterator.h>
#include <Poco/SHA1Engine.h>
#include <adasworks/sx/check.h>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <nowide/fstream.hpp>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "misc_utils.h"
#include "print.h"

CEREAL_CLASS_VERSION(cmakex::pkg_desc_t, 1)
CEREAL_CLASS_VERSION(cmakex::pkg_build_pars_t, 1)
CEREAL_CLASS_VERSION(cmakex::pkg_clone_pars_t, 1)
CEREAL_CLASS_VERSION(cmakex::pkg_files_t, 1)

namespace cmakex {

namespace fs = filesystem;

#define A(X) cereal::make_nvp(#X, m.X)

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
void serialize(Archive& archive, pkg_desc_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(name), A(c), A(b), A(depends));
}

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

#undef A

InstallDB::InstallDB(string_par binary_dir)
    : binary_dir(binary_dir.str()),
      dbpath(cmakex_config_t(binary_dir).cmakex_dir() + "/" + "installed")
{
    if (!fs::exists(dbpath))
        fs::create_directories(dbpath);  // must be able to create the path
}

maybe<pkg_desc_t> InstallDB::try_get_installed_pkg_desc(string_par pkg_name) const
{
    auto path = installed_pkg_desc_path(pkg_name);
    nowide::ifstream f(path);
    if (!f.good())
        return nothing;
    string what;
    try {
        // otherwise it must succeed
        cereal::JSONInputArchive a(f);
        maybe<pkg_desc_t> r(in_place);
        a(*r);
        return r;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't read installed package descriptor \"%s\", reason: %s", path.c_str(),
           what.c_str());
    // never here
    return nothing;
}

maybe<pkg_files_t> InstallDB::try_get_installed_pkg_files(string_par pkg_name) const
{
    auto path = installed_pkg_files_path(pkg_name);
    nowide::ifstream f(path);
    if (!f.good())
        return nothing;
    string what;
    try {
        // otherwise it must succeed
        cereal::JSONInputArchive a(f);
        maybe<pkg_files_t> r(in_place);
        a(*r);
        return r;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't read installed package file list \"%s\", reason: %s", path.c_str(), what.c_str());
    // never here
    return nothing;
}

void InstallDB::put_installed_pkg_desc(const pkg_desc_t& p)
{
    auto path = installed_pkg_desc_path(p.name);
    nowide::ofstream f(path, std::ios_base::trunc);
    if (!f.good())
        throwf("Can't open installed package descriptor \"%s\" for writing.", path.c_str());
    string what;
    try {
        cereal::JSONOutputArchive a(f);
        a(p);
        f.close();
        return;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't write installed package descriptor \"%s\", reason: %s", path.c_str(),
           what.c_str());
    // never here
}

string InstallDB::installed_pkg_desc_path(string_par pkg_name) const
{
    return dbpath + "/" + pkg_name.c_str() + ".json";
}

string InstallDB::installed_pkg_files_path(string_par pkg_name) const
{
    return dbpath + "/" + pkg_name.c_str() + "-files.json";
}

string varname_from_dash_d_cmake_arg(const string& x)
{
    CHECK(starts_with(x, "-D"));
    auto y = butleft(x, 2);
    auto p = std::min(std::find(BEGINEND(y), ':'), std::find(BEGINEND(y), '='));
    if (p == y.end())
        throwf("Invalid CMAKE_ARG: '%s'", x.c_str());
    return string(y.begin(), p);
}

vector<string> make_canonical_cmake_args(const vector<string>& x)
{
    std::unordered_map<string, string> prev_options;
    vector<string> canonical_args;
    std::unordered_map<string, string>
        varname_to_last_arg;  // maps variable name to the last option that deals with that variable
    for (auto& a : x) {
        bool found = false;
        for (auto o : {"-C", "-G", "-T", "-A"}) {
            if (starts_with(a, o)) {
                found = true;
                auto& prev_option = prev_options[o];
                if (prev_option.empty()) {
                    prev_option = a;
                    canonical_args.emplace_back(a);
                } else {
                    if (prev_option != a) {
                        throwf(
                            "Two, different '%s' options has been specified: \"%s\" and \"%s\". "
                            "There should be only a single '%s' option for a build.",
                            o, prev_option.c_str(), a.c_str(), o);
                    }
                }
                break;
            }
        }
        if (!found) {
            if (starts_with(a, "-D")) {
                auto varname = varname_from_dash_d_cmake_arg(a);
                if (varname.empty())
                    throwf("Invalid CMAKE_ARG: %s", a.c_str());
                varname_to_last_arg[varname] = a;
            } else if (starts_with(a, "-U")) {
                auto varname = butleft(a, 2);
                if (varname.empty())
                    throwf("Invalid CMAKE_ARG: %s", a.c_str());
                varname_to_last_arg[make_string(varname)] = a;
            }
        }
    }
    for (auto& kv : varname_to_last_arg)
        canonical_args.emplace_back(kv.second);
    std::sort(BEGINEND(canonical_args));
    return canonical_args;
}

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
    auto cx = make_canonical_cmake_args(x);
    auto cy = make_canonical_cmake_args(y);
    for (auto& o : set_difference(cx, cy))
        if (is_critical_cmake_arg(o))
            r.emplace_back(o);
    for (auto& o : set_difference(cy, cx))
        if (is_critical_cmake_arg(o))
            r.emplace_back(o);
    return r;
}

pkg_request_eval_details_t InstallDB::evaluate_pkg_request(const pkg_desc_t& req)
{
    auto maybe_desc = try_get_installed_pkg_desc(req.name);
    pkg_request_eval_details_t r;
    if (!maybe_desc)
        r.status = pkg_request_not_installed;
    else {
        r.pkg_desc = move(*maybe_desc);
        auto ica = incompatible_cmake_args(maybe_desc->b.cmake_args, req.b.cmake_args);
        if (ica.empty()) {
            r.missing_configs = set_difference(req.b.configs, maybe_desc->b.configs);
            if (r.missing_configs.empty())
                r.status = pkg_request_satisfied;
            else {
                r.status = pkg_request_missing_configs;
            }
        } else {
            r.status = pkg_request_not_compatible;
            r.incompatible_cmake_args = join(ica, ", ");
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

void InstallDB::install(pkg_desc_t desc)
{
    auto maybe_desc = try_get_installed_pkg_desc(desc.name);
    if (maybe_desc)
        uninstall(maybe_desc->name);
    // install files, that is copy from pkg install dir to deps install dir (use symlinks if
    // possible)
    cmakex_config_t cfg(binary_dir);

    auto pkg_install_dir = cfg.pkg_install_dir(desc.name);
    auto deps_install_dir = cfg.deps_install_dir();

    pkg_files_t pkg_files;

    Poco::SHA1Engine e;
    auto files = enumerate_files_recursively(pkg_install_dir);
    std::sort(BEGINEND(files));
    for (auto& f : files) {
        pkg_files_t::file_item_t item;
        CHECK(starts_with(f, pkg_install_dir));
        // todo make certain files relocatable and calc the sha for updated file
        item.path = make_string(butleft(f, pkg_install_dir.size()));
        item.sha = file_sha(f);
        e.update(item.path.data(), item.path.size());
        e.update(item.sha.data(), item.sha.size());

        Poco::File(f).copyTo(deps_install_dir + "/" + item.path);
        // todo create symlink or move?
    }

    // update pkg_desc along the way
    desc.sha_of_installed_files = Poco::DigestEngine::digestToHex(e.digest());
    // write out pkg_desc and file desc jsons
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

void InstallDB::uninstall(string_par pkg_name)
{
    auto maybe_files = try_get_installed_pkg_files(pkg_name);
    CHECK(maybe_files);
    auto& files = *maybe_files;
    // remove the files
    for (auto& f : files.files)
        remove_and_log_error(f.path);
    // remove the jsons
    remove_and_log_error(installed_pkg_desc_path(pkg_name));
    remove_and_log_error(installed_pkg_files_path(pkg_name));
}
}
