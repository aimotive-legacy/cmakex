#include "installdb.h"

#include <Poco/DirectoryIterator.h>
#include <Poco/SHA1Engine.h>
#include <adasworks/sx/algorithm.h>
#include <adasworks/sx/check.h>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
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
    archive(A(name), A(c), A(b), A(depends), A(sha_of_installed_files), A(dep_shas));
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
      dbpath(cmakex_config_t(binary_dir).cmakex_dir() + "/" + "installdb")
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

void InstallDB::put_installed_pkg_desc(pkg_desc_t p)
{
    p.b.cmake_args = make_canonical_cmake_args(p.b.cmake_args);
    auto path = installed_pkg_desc_path(p.name);
    nowide::ofstream f(path, std::ios_base::trunc);
    if (!f.good())
        throwf("Can't open installed package descriptor \"%s\" for writing.", path.c_str());
    string what;
    try {
        cereal::JSONOutputArchive a(f);
        a(p);
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

void InstallDB::put_installed_pkg_files(string_par pkg_name, const pkg_files_t& p)
{
    auto path = installed_pkg_files_path(pkg_name);
    nowide::ofstream f(path, std::ios_base::trunc);
    if (!f.good())
        throwf("Can't open installed package files \"%s\" for writing.", path.c_str());
    string what;
    try {
        cereal::JSONOutputArchive a(f);
        a(p);
        return;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't write installed package files \"%s\", reason: %s", path.c_str(), what.c_str());
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

pkg_request_eval_details_t InstallDB::evaluate_pkg_request_build_pars(string_par pkg_name,
                                                                      const pkg_build_pars_t& bp)
{
    auto maybe_desc = try_get_installed_pkg_desc(pkg_name);
    pkg_request_eval_details_t r;
    if (!maybe_desc)
        r.status = pkg_request_not_installed;
    else {
        r.pkg_desc = move(*maybe_desc);
        auto ica = incompatible_cmake_args(r.pkg_desc.b.cmake_args, bp.cmake_args);
        if (bp.source_dir != r.pkg_desc.b.source_dir) {
            ica.emplace_back(stringf("(different source dirs: '%s' and '%s')",
                                     r.pkg_desc.b.source_dir.c_str(), bp.source_dir.c_str()));
        }
        if (ica.empty()) {
            r.missing_configs = set_difference(bp.configs, r.pkg_desc.b.configs);
            if (r.missing_configs.empty())
                r.status = pkg_request_satisfied;
            else {
                r.status = pkg_request_missing_configs;
            }
        } else {
            r.status = pkg_request_not_compatible;
            r.incompatible_cmake_args = join(ica, " ");
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

void InstallDB::install(pkg_desc_t desc, bool incremental)
{
    auto maybe_desc = try_get_installed_pkg_desc(desc.name);

    if (maybe_desc) {
        if (incremental) {
            // check build pars
            // this and other attributes (clone SHA) are checked by the caller code, this is only an
            // extra sanity check
            auto epr = evaluate_pkg_request_build_pars(desc.name, desc.b);
            switch (epr.status) {
                case pkg_request_not_installed:
                    LOG_FATAL(
                        "Internal error: %s reported installed first then not installed "
                        "shortly after.",
                        pkg_for_log(desc.name).c_str());
                case pkg_request_satisfied:
                case pkg_request_missing_configs:
                    break;
                case pkg_request_not_compatible:
                    LOG_FATAL(
                        "Internal error: incremental install is only possible when "
                        "the build settings are compatible. Incompatible settings: %s",
                        epr.incompatible_cmake_args.c_str());
                    break;
                default:
                    CHECK(false);
            }

            // extend installed configs
            desc.b.configs.insert(desc.b.configs.end(), BEGINEND(maybe_desc->b.configs));
            std::sort(BEGINEND(desc.b.configs));
            sx::unique_trunc(desc.b.configs);
        } else {
            uninstall(maybe_desc->name);
        }
    }
    // install files, that is copy from pkg install dir to deps install dir (use symlinks if
    // possible)
    cmakex_config_t cfg(binary_dir);

    auto pkg_install_dir = cfg.pkg_install_dir(desc.name);
    auto deps_install_dir = cfg.deps_install_dir();

    pkg_files_t pkg_files;

    if (incremental && maybe_desc)
        pkg_files = *try_get_installed_pkg_files(desc.name);

    auto files = enumerate_files_recursively(pkg_install_dir);
    std::sort(BEGINEND(files));
    for (auto& f : files) {
        pkg_files_t::file_item_t item;
        CHECK(starts_with(f, pkg_install_dir) && f.size() > pkg_install_dir.size() + 1);
        // todo make certain files relocatable and calc the sha for updated file
        item.path = make_string(butleft(f, pkg_install_dir.size() + 1));
        item.sha = file_sha(f);
        auto target_path = deps_install_dir + "/" + item.path;
        Poco::File(Poco::Path(target_path).parent()).createDirectories();
        Poco::File(f).copyTo(deps_install_dir + "/" + item.path);
        // todo create symlink or move?
        pkg_files.files.emplace_back(move(item));
    }
    std::sort(BEGINEND(pkg_files.files), &pkg_files_t::file_item_t::less_path);
    sx::unique_trunc(pkg_files.files, &pkg_files_t::file_item_t::equal_path);
    Poco::SHA1Engine e;
    for (auto& item : pkg_files.files) {
        e.update(item.path.data(), item.path.size());
        e.update(item.sha.data(), item.sha.size());
    }

    // update pkg_desc along the way
    desc.sha_of_installed_files = Poco::DigestEngine::digestToHex(e.digest());
    // write out pkg_desc and file desc jsons

    put_installed_pkg_desc(desc);
    put_installed_pkg_files(desc.name, pkg_files);
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
    cmakex_config_t cfg(binary_dir);
    auto deps_install_dir = cfg.deps_install_dir();
    for (auto& f : files.files) {
        remove_and_log_error(deps_install_dir + "/" + f.path);
    }
    // remove the jsons
    remove_and_log_error(installed_pkg_desc_path(pkg_name));
    remove_and_log_error(installed_pkg_files_path(pkg_name));
}
void cmakex_cache_save(string_par pkg_bin_dir,
                       string_par pkg_name,
                       const vector<string>& cmake_args_in)
{
    fs::create_directories(pkg_bin_dir.c_str());
    auto path = pkg_bin_dir.str() + "/" + k_pkg_cmakex_cache_filename;

    auto cmake_args = make_canonical_cmake_args(cmake_args_in);

    nowide::ofstream f(path, std::ios_base::trunc);
    if (!f.good())
        throwf("Can't open cmakex cache file \"%s\" for writing.", path.c_str());

    string what;
    try {
        cereal::JSONOutputArchive a(f);
        a(cmake_args);
        return;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't write cmakex cache file \"%s\", reason: %s", path.c_str(), what.c_str());
    // never here}
}

vector<string> cmakex_cache_load(string_par pkg_bin_dir, string_par name)
{
    auto path = pkg_bin_dir.str() + "/" + k_pkg_cmakex_cache_filename;

    nowide::ifstream f(path);
    if (!f.good())
        return {};

    string what;
    try {
        // otherwise it must succeed
        cereal::JSONInputArchive a(f);
        vector<string> v;
        a(v);
        return v;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't read cmakex cache file \"%s\", reason: %s", path.c_str(), what.c_str());
    // never here
    return {};
}
}
