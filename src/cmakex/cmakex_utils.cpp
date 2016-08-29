#include "cmakex_utils.h"

#include <adasworks/sx/algorithm.h>

#include "cereal_utils.h"
#include "filesystem.h"
#include "misc_utils.h"
#include "print.h"

CEREAL_CLASS_VERSION(cmakex::cmakex_cache_t, 1)
CEREAL_CLASS_VERSION(cmakex::cmake_cache_tracker_t, 1)

namespace cmakex {

namespace fs = filesystem;

#define A(X) cereal::make_nvp(#X, m.X)

template <class Archive>
void serialize(Archive& archive, cmakex_cache_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(valid), A(home_directory), A(multiconfig_generator), A(per_config_bin_dirs),
            A(cmakex_prefix_path_vector), A(env_cmakex_prefix_path_vector));
}

template <class Archive>
void serialize(Archive& archive, cmake_cache_tracker_t::var_t& m)
{
    archive(A(value), A(status));
}

template <class Archive>
void serialize(Archive& archive, cmake_cache_tracker_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(vars), A(c_sha), A(cmake_toolchain_file_sha));
}

#undef A

cmakex_config_t::cmakex_config_t(string_par cmake_binary_dir)
    : cmake_binary_dir(fs::absolute(cmake_binary_dir.c_str()).string())
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
        load_json_input_archive(path, cmakex_cache_);
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

string cmakex_config_t::main_binary_dir_of_config(const config_name_t& config,
                                                  bool per_config_bin_dirs) const
{
    string r = cmake_binary_dir;
    if (!config.is_noconfig() && per_config_bin_dirs)
        r += "/" + config.get_prefer_NoConfig();
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
                                                 const config_name_t& config,
                                                 bool per_config_bin_dirs) const
{
    auto r = pkg_binary_dir_common(pkg_name);
    if (!config.is_noconfig() && per_config_bin_dirs)
        r += "/" + config.get_prefer_NoConfig();
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

parsed_cmake_arg_t parse_cmake_arg(string_par x)
{
    auto throw_if_msg = [&x](bool cond, const char* reason) {
        if (cond) {
            if (reason)
                throwf("Invalid CMAKE_ARG: '%s', reason: %s", x.c_str(), reason);
            else
                throwf("Invalid CMAKE_ARG: '%s'", x.c_str());
        }
    };

    auto throw_if = [&throw_if_msg](bool cond) { throw_if_msg(cond, nullptr); };

    const char* pos_end = strchr(x.c_str(), 0);

    parsed_cmake_arg_t r;

    for (const char* c : {"-C", "-G", "-T", "-A"}) {
        if (starts_with(x, c)) {
            r.switch_ = c;
            r.value.assign(x.c_str() + strlen(c), pos_end);
            throw_if(r.value.empty());
            return r;
        }
    }

    if (starts_with(x, "-U")) {
        r.switch_ = "-U";
        r.name.assign(x.c_str() + 2, pos_end);
        throw_if(r.name.empty());
        return r;
    }

    if (starts_with(x, "-D")) {
        r.switch_ = "-D";

        const char* pos_name = x.c_str() + 2;
        const char* pos_equal = strchr(pos_name, '=');

        throw_if_msg(!pos_equal, "no equal sign");

        const char* pos_colon = strchr(pos_name, ':');

        if (!pos_colon)
            pos_colon = pos_end;
        else if (pos_colon > pos_equal)
            pos_colon = pos_end;

        const char* pos_name_end = std::min(pos_colon, pos_equal);

        r.name.assign(pos_name, pos_name_end);

        throw_if_msg(r.name.empty(), "missing variable name");

        if (pos_colon < pos_equal)
            r.type.assign(pos_colon + 1, pos_equal);

        r.value.assign(pos_equal + 1, pos_end);

        return r;
    }

    for (const char* c :
         {"-Wno-dev", "-Wdev", "-Werror=dev", "Wno-error=dev", "-Wdeprecated", "-Wno-deprecated",
          "-Werror=deprecated", "-Wno-error=deprecated", "-N", "--debug-trycompile",
          "--debug-output", "--trace", "--trace-expand", "--warn-uninitialized",
          "--warn-unused-vars", "--no-warn-unused-cli", "--check-system-vars"}) {
        if (x == c) {
            r.switch_ = c;
            return r;
        }
    }

    {
        const char* word = "--graphwiz=";
        if (starts_with(x, word)) {
            r.switch_ = "--graphwiz";
            r.value.assign(x.c_str() + strlen(word), pos_end);
            return r;
        }
    }

    throw_if(true);

    // never here;

    return r;
}

string extract_generator_from_cmake_args(const vector<string>& cmake_args)
{
    vector<string> cmake_generators;
    for (auto& x : cmake_args) {
        auto pca = parse_cmake_arg(x);
        if (pca.switch_ == "-G" || (pca.switch_ == "-D" && pca.name == "CMAKE_GENERATOR")) {
            cmake_generators.emplace_back(pca.value);
        }
    }
    if (cmake_generators.empty())
        return {};
    THROW_IF(cmake_generators.size() > 1, "Multiple CMake-generators specified: %s",
             join(cmake_generators, ", ").c_str());
    return cmake_generators.front();
}

template <class X, class UnaryOp>
void transform_inplace(X& x, UnaryOp uo)
{
    for (auto& y : x)
        uo(y);
}

template <class X, class UnaryOp>
bool all_of(X& x, UnaryOp uo)
{
    for (auto& y : x)
        if (!uo(y))
            return false;
    return true;
}

bool eval_cmake_boolean_or_fail(string_par x)
{
    string s = x.str();
    transform_inplace(x, ::tolower);
    if (s == "1" || s == "on" || s == "yes" || s == "true" ||
        (!s.empty() && s[0] != '0' && isdigit(s[0]) && all_of(s, ::isdigit)))
        return true;
    if (s == "0" || s == "off" || s == "false" || s == "n" || s == "ignore" || s == "notfound" ||
        s.empty() || ends_with(s, "-notfound"))
        return false;
    throwf("Invalid boolean constant: %s", x.c_str());
}

pkg_request_t pkg_request_from_arg_str(const string& pkg_arg_str,
                                       const vector<config_name_t>& default_configs)
{
    return pkg_request_from_args(separate_arguments(pkg_arg_str), default_configs);
}

// clang-format off
/* todo add support for local repos (no GIT_URL case)
SOURCE_DIR GIT_URL
    abs     yes          ERROR
    abs     no           OK, local project, no clone: SOURCE_DIR/CMakeLists.txt
    rel     yes          OK, clone_dir/SOURCE_DIR/CMakeLists.txt
    rel     no           OK, local project, no clone: parent_path(deps.cmake)/SOURCE_DIR/CMakeLists.txt
    no      yes          OK, clone_dir/CMakeLists.txt
    no      no           ERROR
*/
// clang-format on

pkg_request_t pkg_request_from_args(const vector<string>& pkg_args,
                                    const vector<config_name_t>& default_configs)
{
    CHECK(!default_configs.empty());
    if (pkg_args.empty())
        throwf("Empty package descriptor, package name is missing.");
    const string request_name = pkg_args[0];
    const auto args = parse_arguments(
        {}, {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "SOURCE_DIR", "GIT_SHALLOW"},
        {"DEPENDS", "CMAKE_ARGS", "CONFIGS"}, vector<string>(pkg_args.begin() + 1, pkg_args.end()));

    if (args.count("GIT_URL") == 0 && args.count("GIT_REPOSITORY") == 0) {
        // this supposed to be a name-only request. No other fields should be specified
        for (auto c :
             {"GIT_TAG", "SOURCE_DIR", "GIT_SHALLOW", "DEPENDS", "CMAKE_ARGS", "CONFIGS"}) {
            if (args.count(c) != 0)
                throwf(
                    "Missing GIT_URL/GIT_REPOSITORY. It should be either only the package name or "
                    "the package name with GIT_URL/GIT_REPOSITORY and optional other arguments.");
        }
    }

    vector<config_name_t> configs;
    if (args.count("CONFIGS") > 0) {
        for (auto& cc : args.at("CONFIGS")) {
            auto c = trim(cc);
            if (c.empty())
                throwf("Empty configuration name is invalid.");
            configs.emplace_back(c);
        }
    }
    bool using_default_configs = false;
    if (configs.empty()) {
        configs = default_configs;
        using_default_configs = true;
    }

    pkg_request_t request(pkg_args[0], stable_unique(configs), using_default_configs);

    for (auto c : {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "SOURCE_DIR"}) {
        auto count = args.count(c);
        CHECK(count == 0 || args.at(c).size() == 1);
        if (count > 0 && args.at(c).empty())
            throwf("Empty string after '%s'.", c);
    }
    string a, b;
    if (args.count("GIT_REPOSITORY") > 0)
        a = args.at("GIT_REPOSITORY")[0];
    if (args.count("GIT_URL") > 0)
        b = args.at("GIT_URL")[0];
    if (!a.empty()) {
        request.c.git_url = a;
        if (!b.empty())
            throwf("Both GIT_URL and GIT_REPOSITORY are specified.");
    } else
        request.c.git_url = b;

    if (args.count("GIT_TAG") > 0)
        request.c.git_tag = args.at("GIT_TAG")[0];
    if (args.count("GIT_SHALLOW") > 0)
        request.git_shallow = eval_cmake_boolean_or_fail(args.at("GIT_SHALLOW")[0]);
    if (args.count("SOURCE_DIR") > 0) {
        request.b.source_dir = args.at("SOURCE_DIR")[0];
        if (fs::path(request.b.source_dir).is_absolute())
            throwf("SOURCE_DIR must be a relative path: \"%s\"", request.b.source_dir.c_str());
    }
    if (args.count("DEPENDS") > 0) {
        for (auto& d : args.at("DEPENDS"))
            request.depends.insert(d);  // insert empty string
    }
    if (args.count("CMAKE_ARGS") > 0) {
        // join some cmake options for easier search
        request.b.cmake_args = normalize_cmake_args(args.at("CMAKE_ARGS"));
        for (auto& a : request.b.cmake_args) {
            auto pca = parse_cmake_arg(a);
            if (pca.switch_ == "-D" &&
                is_one_of(pca.name,
                          {"CMAKE_INSTALL_PREFIX", "CMAKE_PREFIX_PATH", "CMAKE_MODULE_PATH"}))
                throwf("Setting '%s' is not allowed in the CMAKE_ARGS of a package request",
                       pca.name.c_str());
        }
    }
    return request;
}

vector<string> normalize_cmake_args(const vector<string>& x)
{
    vector<string> y;
    y.reserve(x.size());

    for (auto it = x.begin(); it != x.end(); ++it) {
        y.emplace_back(*it);
        if (it->size() == 2 && is_one_of(*it, {"-C", "-D", "-U", "-G", "-T", "-A"})) {
            ++it;
            THROW_IF(it == x.end(), "Missing argument after '%s'", y.back().c_str());
            y.back() += *it;
        }
    }

    std::unordered_map<string, string> prev_options;
    std::map<string, string>
        varname_to_last_arg;  // maps variable name to the last option that deals with that variable
    vector<string> z;
    z.reserve(y.size());
    const char* GTA_vars[] = {"CMAKE_GENERATOR", "CMAKE_GENERATOR_TOOLSET",
                              "CMAKE_GENERATOR_PLATFORM", nullptr};
    for (auto& a : y) {
        auto pca = parse_cmake_arg(a);  // also throws on invalid args

        //-U globbing is not yet supported
        if (pca.switch_ == "-U") {
            if (pca.name.find('*') != string::npos || pca.name.find('?') != string::npos)
                throwf(
                    "Invalid CMAKE_ARG: '%s', globbing characters '*' and '?' are not supported "
                    "for now.",
                    a.c_str());
        }

        // transform to -G, -A, -T form
        if (pca.switch_ == "-U" || pca.switch_ == "-D") {
            int gta_idx = 0;
            for (; GTA_vars[gta_idx]; ++gta_idx) {
                if (pca.name == GTA_vars[gta_idx])
                    break;
            }
            if (pca.switch_ == "-U") {
                if (GTA_vars[gta_idx])  // if found
                    throwf("Invalid CMAKE_ARG: '%s', this variable should not be removed.",
                           a.c_str());
            } else {
                switch (gta_idx) {
                    case 0:
                        a = "-G" + pca.value;
                        break;
                    case 1:
                        a = "-T" + pca.value;
                        break;
                    case 2:
                        a = "-A" + pca.value;
                        break;
                    default:
                        CHECK(gta_idx == 3);
                }
            }

            // pca is not updated above, only 'a'
            bool found = false;
            for (auto o : {"-C", "-G", "-T", "-A"}) {
                if (starts_with(a, o)) {
                    found = true;
                    auto& prev_option = prev_options[o];
                    if (prev_option.empty()) {
                        prev_option = a;
                        z.emplace_back(a);
                    } else {
                        if (prev_option != a) {
                            throwf(
                                "Two, different '%s' options has been specified: \"%s\" and "
                                "\"%s\". "
                                "There should be only a single '%s' option for a build.",
                                o, prev_option.c_str(), a.c_str(), o);
                        }
                    }
                    break;
                }
            }
            if (found)
                continue;

            if (starts_with(a, "-D")) {
                varname_to_last_arg[pca.name] = a;
            } else if (starts_with(a, "-U")) {
                varname_to_last_arg[pca.name] = a;
            } else {
                // non C, D, U, G, T, A option
                z.emplace_back(a);
            }
        }
    }
    for (auto& kv : varname_to_last_arg)
        z.emplace_back(kv.second);
    std::sort(BEGINEND(z));
    sx::unique_trunc(z);
    return z;
}

void write_cmakex_cache_if_dirty(string_par binary_dir, const cmakex_cache_t& cmakex_cache)
{
    cmakex_config_t cfg(binary_dir);
    if (!cfg.cmakex_cache().valid || cfg.cmakex_cache() != cmakex_cache)
        save_json_output_archive(cfg.cmakex_cache_path(), cmakex_cache);
}

CMakeCacheTracker::CMakeCacheTracker(string_par bin_dir)
    : path(bin_dir.str() + "/" + k_cmake_cache_tracker_filename)
{
}

CMakeCacheTracker::CMakeCacheTracker(string_par bin_dir, string_par filename)
    : path(bin_dir.str() + "/" + filename.c_str())
{
}

void update_reference_cmake_cache_tracker(string_par pkg_bin_dir_common,
                                          const vector<string>& cmake_args)
{
    CMakeCacheTracker ccc(pkg_bin_dir_common, k_cmake_cache_tracker_ref_filename);
    ccc.about_to_configure(cmake_args, false);
    ccc.cmake_config_ok();
}

tuple<vector<string>, bool> CMakeCacheTracker::about_to_configure(
    const vector<string>& cmake_args_in,
    bool force_input_cmake_args,
    string_par ref_path)
{
    using var_t = cmake_cache_tracker_t::var_t;
    using var_status_t = cmake_cache_tracker_t::var_status_t;

    auto cmake_args = normalize_cmake_args(cmake_args_in);

    cmake_cache_tracker_t cct;
    if (fs::is_regular_file(path))
        load_json_input_archive(path, cct);

    vector<string> current_request_nonvar_args;
    vector<string> current_request_vars;

    for (auto& c : cmake_args) {
        auto pca = parse_cmake_arg(c);

        if (pca.switch_ == "-U") {
            auto it = cct.vars.find(pca.name);
            if (it != cct.vars.end()) {
                auto& v = it->second;
                v.value = "-U" + pca.name;
                v.status = var_status_t::vs_to_be_removed;
            }
            current_request_vars.emplace_back(pca.name);
            continue;
        }

        string name, value;
        if (pca.switch_ == "-D") {
            name = pca.name;
            value = c;
        } else if (is_one_of(pca.switch_, {"-C", "-G", "-T", "-A"})) {
            name = pca.switch_;
            value = c;
        } else {
            current_request_nonvar_args.emplace_back(c);
            continue;
        }

        current_request_vars.emplace_back(pca.name);

        auto it = cct.vars.find(name);

        if (name == "-G" || name == "-T" || name == "-A") {
            if (it != cct.vars.end() && it->second.value != value)
                throwf(
                    "The '%s' switch can be set only once. Previous value: '%s', current "
                    "request: '%s'.",
                    name.c_str(), it->second.value.c_str(), value.c_str());
        }

        if (it == cct.vars.end()) {
            cct.vars.emplace(name, var_t{value, var_status_t::vs_to_be_defined});
        } else if (it->second.value != value) {
            it->second.value = value;
            it->second.status = var_status_t::vs_to_be_defined;
        }
    }

    if (!ref_path.empty()) {
        cmake_cache_tracker_t ref;
        CHECK(fs::is_regular_file(path));
        load_json_input_archive(ref_path, ref);
        // merge to two vars
        auto it = cct.vars.begin();
        auto jt = ref.vars.begin();
        for (; it != cct.vars.end() || jt != ref.vars.end();) {
            if (jt == ref.vars.end() || it->first < jt->first) {
                // there's a var here which is not listed in the reference
                if (it->second.status != var_status_t::vs_to_be_removed) {
                    // must be removed
                    it->second.value = "-U" + it->first;
                    it->second.status = var_status_t::vs_to_be_removed;
                }
                ++it;
            } else if (it == cct.vars.end() || jt->first < it->first) {
                // there's a var in reference which is not listed here
                if (jt->second.status != var_status_t::vs_to_be_removed) {
                    cct.vars.emplace(jt->first,
                                     var_t{jt->second.value, var_status_t::vs_to_be_defined});
                }
                ++jt;
            } else {
                CHECK(it->first == jt->first);

                if (jt->second.status == var_status_t::vs_to_be_removed) {
                    it->second = jt->second;
                } else if (it->second.value != jt->second.value) {
                    it->second.value = jt->second.value;
                    it->second.status = var_status_t::vs_to_be_removed;
                }
                ++it;
                ++jt;
            }
        }
    }

    // add placeholder -G, -T, -A to prevent redefinition later if, for the first time it hasn't
    // been initialized with some valid value
    for (auto s : {"-G", "-T", "-A"}) {
        auto it = cct.vars.find(s);
        if (it == cct.vars.end())
            cct.vars.emplace(s, var_t{"", var_status_t::vs_to_be_defined});
    }

    // save -C and CMAKE_TOOLCHAIN_FILE SHA's
    cct.c_sha.clear();
    cct.cmake_toolchain_file_sha.clear();

    auto it = cct.vars.find("-C");
    if (it != cct.vars.end()) {
        auto pca = parse_cmake_arg(it->second.value);
        cct.c_sha = file_sha(pca.value);
    }

    it = cct.vars.find("CMAKE_TOOLCHAIN_FILE");
    if (it != cct.vars.end()) {
        auto pca = parse_cmake_arg(it->second.value);
        if (pca.switch_ == "-D")
            cct.cmake_toolchain_file_sha = file_sha(pca.value);
    }

    // remember CMAKE_BUILD_TYPE state
    bool cmake_build_type_changing = false;
    it = cct.vars.find("CMAKE_BUILD_TYPE");
    if (it != cct.vars.end()) {
        auto pca = parse_cmake_arg(it->second.value);
        if (pca.switch_ == "-D")
            cmake_build_type_changing =
                it->second.status != cmake_cache_tracker_t::vs_in_cmakecache;
    }

    fs::create_directories(fs::path(path).parent_path());
    save_json_output_archive(path, cct);

    // update the variables that differs from the assumed state plus the current request
    std::sort(BEGINEND(current_request_vars));
    vector<string> cmake_args_to_apply;
    for (auto& kv : cct.vars) {
        auto& name = kv.first;
        auto& var = kv.second;

        // if we're about to return all the input cmake_args we don't want to list those in
        // cmake_args_to_apply, too
        if (force_input_cmake_args && std::binary_search(BEGINEND(current_request_vars), name))
            continue;

        // don't list here if it does not need to be changed
        if (var.status == var_status_t::vs_in_cmakecache)
            continue;

        if (!var.value.empty() || !(name == "-G" || name == "-T" || name == "-A"))
            cmake_args_to_apply.emplace_back(kv.second.value);
    }

    if (force_input_cmake_args)
        append_inplace(cmake_args_to_apply, cmake_args);
    else
        append_inplace(cmake_args_to_apply, current_request_nonvar_args);

    return make_tuple(normalize_cmake_args(cmake_args_to_apply), cmake_build_type_changing);
}

CMakeCacheTracker::report_t CMakeCacheTracker::cmake_config_ok()
{
    cmake_cache_tracker_t cct;
    load_json_input_archive(path, cct);
    report_t r;

    auto remember_cmake_prefix_path = [&r](decltype(cct.vars)::iterator it) {
        if (it->first == "CMAKE_PREFIX_PATH") {
            auto a = parse_cmake_arg(it->second.value);
            if (a.switch_ == "-D" && a.name == "CMAKE_PREFIX_PATH") {
                r.cmake_prefix_path = a.value;
            }
        }
    };

    for (auto it = cct.vars.begin(); it != cct.vars.end(); /* no incr */) {
        auto& v = it->second;

        switch (v.status) {
            case cmake_cache_tracker_t::vs_to_be_removed:
                it = cct.vars.erase(it);
                break;
            case cmake_cache_tracker_t::vs_in_cmakecache:
                remember_cmake_prefix_path(it);
                ++it;
                break;
            case cmake_cache_tracker_t::vs_to_be_defined:
                v.status = cmake_cache_tracker_t::vs_in_cmakecache;
                remember_cmake_prefix_path(it);
                ++it;
                break;
            default:
                CHECK(false);
        }
    }

    save_json_output_archive(path, cct);

    return r;
}

vector<string> cmake_args_prepend_cmake_prefix_path(vector<string> cmake_args, string_par dir)
{
    for (auto& c : cmake_args) {
        auto pca = parse_cmake_arg(c);
        if (pca.switch_ == "-D" && pca.name == "CMAKE_PREFIX_PATH") {
            c = "-DCMAKE_PREFIX_PATH";
            if (!pca.type.empty())
                c += ":" + pca.type;
            c += "=" + dir.str() + ";" + pca.value;
            return cmake_args;
        }
    }
    cmake_args.emplace_back("-DCMAKE_PREFIX_PATH=" + dir.str());
    return normalize_cmake_args(cmake_args);
}

vector<string> cmakex_prefix_path_to_vector(string_par x)
{
    vector<string> r;
#ifdef _WIN32
    const char separator = ';';
#else
    const char separator = ':';
#endif
    auto v = split(x, separator);
    for (auto& p : v) {
        if (!p.empty())
            r.emplace_back(p);
    }
    return r;
}
}
