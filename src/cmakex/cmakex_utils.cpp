#include "cmakex_utils.h"

#include <adasworks/sx/algorithm.h>

#include "cereal_utils.h"
#include "filesystem.h"
#include "misc_utils.h"
#include "print.h"

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

string cmakex_config_t::main_binary_dir_of_config(string_par config, bool per_config_bin_dirs) const
{
    string r = cmake_binary_dir;
    if (!config.empty() && per_config_bin_dirs)
        r += "/" + config.str();
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
                                                 bool per_config_bin_dirs) const
{
    auto r = pkg_binary_dir_common(pkg_name);
    if (!config.empty() && per_config_bin_dirs)
        r += "/" + config.str();
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

#if 0
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

#endif
string pkg_for_log(string_par pkg_name)
{
    return stringf("[%s]", pkg_name.c_str());
}
string same_or_NoConfig(string_par config)
{
    return config.empty() ? "NoConfig" : config.str();
}

parsed_cmake_arg_t parse_cmake_arg(string_par x)
{
    auto throw_if = [&x](bool cond, const char* reason = nullptr) {
        if (cond) {
            if (reason)
                throwf("Invalid CMAKE_ARG: '%s', reason: %s", x.c_str(), reason);
            else
                throwf("Invalid CMAKE_ARG: '%s'", x.c_str());
        }
    };

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

        throw_if(!pos_equal, "no equal sign");

        const char* pos_colon = strchr(pos_name, ':');

        if (!pos_colon)
            pos_colon = pos_end;
        else if (pos_colon > pos_equal)
            pos_colon = pos_end;

        const char* pos_name_end = std::min(pos_colon, pos_equal);

        r.name.assign(pos_name, pos_name_end);

        throw_if(r.name.empty(), "missing variable name");

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
            cmake_generators.emplace_back(pca);
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

pkg_request_t pkg_request_from_arg_str(const string& pkg_arg_str)
{
    return pkg_request_from_args(separate_arguments(pkg_arg_str));
}

pkg_request_t pkg_request_from_args(const vector<string>& pkg_args)
{
    if (pkg_args.empty())
        throwf("Empty package descriptor, package name is missing.");
    pkg_request_t request;
    request.name = pkg_args[0];
    auto args = parse_arguments(
        {}, {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "SOURCE_DIR", "GIT_SHALLOW"},
        {"DEPENDS", "CMAKE_ARGS", "CONFIGS"}, vector<string>(pkg_args.begin() + 1, pkg_args.end()));
    for (auto c : {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "SOURCE_DIR"}) {
        auto count = args.count(c);
        CHECK(count == 0 || args[c].size() == 1);
        if (count > 0 && args[c].empty())
            throwf("Empty string after '%s'.", c);
    }
    string a, b;
    if (args.count("GIT_REPOSITORY") > 0)
        a = args["GIT_REPOSITORY"][0];
    if (args.count("GIT_URL") > 0)
        b = args["GIT_URL"][0];
    if (!a.empty()) {
        request.c.git_url = a;
        if (!b.empty())
            throwf("Both GIT_URL and GIT_REPOSITORY are specified.");
    } else
        request.c.git_url = b;

    if (args.count("GIT_TAG") > 0)
        request.c.git_tag = args["GIT_TAG"][0];
    if (args.count("GIT_SHALLOW") > 0)
        request.git_shallow = eval_cmake_boolean_or_fail(args["GIT_SHALLOW"][0]);
    if (args.count("SOURCE_DIR") > 0) {
        request.b.source_dir = args["SOURCE_DIR"][0];
        if (fs::path(request.b.source_dir).is_absolute())
            throwf("SOURCE_DIR must be a relative path: \"%s\"", request.b.source_dir.c_str());
    }
    if (args.count("DEPENDS") > 0)
        request.depends = args["DEPENDS"];
    if (args.count("CMAKE_ARGS") > 0) {
        // join some cmake options for easier search
        request.b.cmake_args = normalize_cmake_args(args["CMAKE_ARGS"]);
        for (auto& a : request.b.cmake_args) {
            auto pca = parse_cmake_arg(a);
            if (pca.switch_ == "-D" &&
                is_one_of(pca.name,
                          {"CMAKE_INSTALL_PREFIX", "CMAKE_PREFIX_PATH", "CMAKE_MODULE_PATH"}))
                throwf("Setting '%s' is not allowed in the CMAKE_ARGS of a package request",
                       pca.name.c_str());
        }
    }
    if (args.count("CONFIGS") > 0)
        request.b.configs = args["CONFIGS"];

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
    if (!cfg.cmakex_cache_loaded() || cfg.cmakex_cache() != cmakex_cache)
        save_json_output_archive(cfg.cmakex_cache_path(), cmakex_cache);
}

CMakeCacheTracker::CMakeCacheTracker(string_par bin_dir)
    : path(bin_dir.str() + "/" + k_cmake_cache_tracker_filename)
{
}

vector<string> CMakeCacheTracker::about_to_configure(const vector<string>& cmake_args_in)
{
    auto cmake_args = normalize_cmake_args(cmake_args_in);

    cmake_cache_tracker_t cct;
    if (fs::is_regular_file(path))
        load_json_input_archive(path, cct);

    vector<string> current_request_vars;

    for (auto& c : cmake_args) {
        auto pca = parse_cmake_arg(c);

        if (pca.switch_ == "-U") {
            if (cct.desired_vars.count(pca.name) > 0) {
                cct.desired_vars.erase(pca.name);
                cct.assumed_vars[pca.name] = "?";
                cct.uncertain_assumed_vars.emplace_back(pca.name);

                current_request_vars.emplace_back(pca.name);
            }
        } else {
            string name;
            string value;
            if (pca.switch_ == "-D") {
                name = pca.name;
                value = c;
            } else if (is_one_of(pca.switch_, {"-C", "-G", "-T", "-A"})) {
                name = pca.switch_;
                value = c;
            } else
                continue;

            if (name == "-G" || name == "-T" || name == "-A") {
                if (cct.desired_vars.count(name) > 0 && cct.desired_vars[name] != value)
                    throwf(
                        "The '%s' switch can be set only once. Previous value: '%s', current "
                        "request: '%s'.",
                        name.c_str(), cct.desired_vars[name].c_str(), value.c_str());
            }

            cct.desired_vars[name] = value;
            cct.assumed_vars[name] = value;
            cct.uncertain_assumed_vars.emplace_back(name);

            current_request_vars.emplace_back(name);
        }
    }
    if (cct.desired_vars.count("-G") == 0) {
        // add the default generator
        cct.desired_vars["-G"] = "-G";
        cct.assumed_vars["-G"] = "-G";
        cct.uncertain_assumed_vars.emplace_back("-G");
        current_request_vars.emplace_back("-G");
    }

    save_json_output_archive(path, cct);

    // update the variables that differs from the assumed state plus the current request
    std::sort(BEGINEND(current_request_vars));
    std::sort(BEGINEND(cct.uncertain_assumed_vars));
    vector<string> y;
    for (auto& kv : cct.desired_vars) {
        // don't list here if it is listed in the current request
        if (std::binary_search(BEGINEND(current_request_vars), kv.first))
            continue;

        // don't list here if it's value equals to the assumed, not uncertain value
        if (cct.assumed_vars.count(kv.first) > 0 && kv.second == cct.assumed_vars[kv.first] &&
            !std::binary_search(BEGINEND(cct.uncertain_assumed_vars), kv.first))
            continue;

        y.emplace_back(kv.second);
    }

    for (auto& kv : cct.assumed_vars) {
        if (cct.desired_vars.count(kv.first) > 0 ||
            std::binary_search(BEGINEND(current_request_vars), kv.first))
            continue;
        auto pca = parse_cmake_arg(kv.second);
        if (pca.switch_ == "-U" || pca.switch_ == "-D")
            y.emplace_back("-U" + pca.name);
        else {
            CHECK(false,
                  "Internal error: an C|G|T|A type assumed var \"%s\" is not a desired var. These "
                  "types of cmake args should not vanish because they cannot be erased like a "
                  "normal variable.",
                  kv.second.c_str());
        }
    }

    // remove "-G"
    for (auto it = y.begin(); it != y.end(); /*no incr*/) {
        if (*it == "-G")
            it = y.erase(it);
        else
            ++it;
    }

    y.insert(y.end(), BEGINEND(cmake_args));

    return normalize_cmake_args(y);
}

void CMakeCacheTracker::cmake_config_ok()  // call after successful cmake-config
{
    cmake_cache_tracker_t cct;
    load_json_input_archive(path, cct);
    cct.assumed_vars = cct.desired_vars;
    cct.uncertain_assumed_vars.clear();
    // also save -C and CMAKE_TOOLCHAIN_FILE SHA's
    cct.c_sha.clear();
    cct.cmake_toolchain_file_sha.clear();
    for (auto& kv : cct.desired_vars) {
        bool c = starts_with(kv.second, "-C");
        bool y = starts_with(kv.second, "-DCMAKE_TOOLCHAIN_FILE");
        if (!c && !y)
            continue;
        auto pca = parse_cmake_arg(kv.second);
        if (pca.switch_ == "-C")
            cct.c_sha = file_sha(pca.value);
        else if (pca.switch_ == "-D" && pca.name == "CMAKE_TOOLCHAIN_FILE")
            cct.cmake_toolchain_file_sha = file_sha(pca.value);
    }
    save_json_output_archive(path, cct);
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
}
