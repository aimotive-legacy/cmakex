#include "installdb.h"

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <nowide/fstream.hpp>

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "misc_utils.h"

CEREAL_CLASS_VERSION(cmakex::pkg_desc_t, 1)

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

#undef A

InstallDB::InstallDB(string_par binary_dir)
    : dbpath(cmakex_config_t(binary_dir).cmakex_dir() + "/" + "installed")
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
}
