#include "cmakex_utils.h"

#include <adasworks/sx/algorithm.h>

#include "cereal_utils.h"
#include "filesystem.h"
#include "misc_utils.h"
#include "print.h"
#include "resource.h"

CEREAL_CLASS_VERSION(cmakex::cmakex_cache_t, 2)
CEREAL_CLASS_VERSION(cmakex::cmake_cache_tracker_t, 2)

namespace cmakex {

namespace fs = filesystem;

#define A(X) cereal::make_nvp(#X, m.X)

template <class Archive>
void serialize(Archive& archive, cmakex_cache_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1 || version == 2);
    archive(A(valid), A(home_directory), A(multiconfig_generator), A(per_config_bin_dirs),
            A(cmakex_prefix_path_vector), A(env_cmakex_prefix_path_vector), A(cmake_root));
    if (version == 2)
        archive(A(deps_source_dir), A(deps_build_dir), A(deps_install_dir));
}

template <class Archive>
void serialize(Archive& archive, cmake_cache_tracker_t& m, uint32_t version)
{
    THROW_UNLESS(version == 2);
    archive(A(pending_cmake_args), A(cached_cmake_args), A(c_sha), A(cmake_toolchain_file_sha));
}

#undef A

string cmake_cache_tracker_path(string_par bin_dir)
{
    return bin_dir.str() + "/" + k_cmake_cache_tracker_filename;
}

void remove_cmake_cache_tracker(string_par bin_dir)
{
    auto p = cmake_cache_tracker_path(bin_dir);
    if (fs::is_regular_file(p))
        fs::remove(p);
}

cmake_cache_tracker_t load_cmake_cache_tracker(string_par bin_dir)
{
    cmake_cache_tracker_t x;
    auto p = cmake_cache_tracker_path(bin_dir);
    if (!fs::is_regular_file(p))
        return x;
    load_json_input_archive(p, x);
    return x;
}

void cmake_cache_tracker_t::add_pending(const vector<string>& cmake_args)
{
    pending_cmake_args = normalize_cmake_args(concat(pending_cmake_args, cmake_args));
}

void cmake_cache_tracker_t::confirm_pending()
{
    for (auto& a : pending_cmake_args) {
        auto pca = parse_cmake_arg(a);

        if (pca.switch_ == "-C") {
            c_sha = file_sha(pca.value);
        } else if (pca.name == "CMAKE_TOOLCHAIN_FILE") {
            if (pca.switch_ == "-D") {
                cmake_toolchain_file_sha = file_sha(pca.value);
            } else if (pca.switch_ == "-U") {
                cmake_toolchain_file_sha.clear();
            } else {
                LOG_FATAL("Internal error: invalid arg in cmake cache tracker: %s", a.c_str());
            }
        }
    }
    cached_cmake_args = normalize_cmake_args(concat(cached_cmake_args, pending_cmake_args));
    pending_cmake_args.clear();
}

void save_cmake_cache_tracker(string_par bin_dir, const cmake_cache_tracker_t& x)
{
    save_json_output_archive(cmake_cache_tracker_path(bin_dir), x);
}

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
    if (per_config_bin_dirs)
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
    return (cmakex_cache_.deps_source_dir.empty() ? default_deps_source_dir()
                                                  : cmakex_cache_.deps_source_dir + "/") +
           pkg_name.c_str();
}

string cmakex_config_t::pkg_binary_dir_common(string_par pkg_name) const
{
    return (cmakex_cache_.deps_build_dir.empty() ? default_deps_build_dir()
                                                 : cmakex_cache_.deps_build_dir + "/") +
           pkg_name.c_str() + "-build";
}

string cmakex_config_t::pkg_binary_dir_of_config(string_par pkg_name,
                                                 const config_name_t& config,
                                                 bool per_config_bin_dirs) const
{
    auto r = pkg_binary_dir_common(pkg_name);
    if (per_config_bin_dirs)
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

string cmakex_config_t::deps_install_dir() const
{
    return cmakex_cache_.deps_install_dir.empty() ? default_deps_install_dir()
                                                  : cmakex_cache_.deps_install_dir;
}

string cmakex_config_t::find_module_hijack_dir() const
{
    return deps_install_dir() + "/_cmakex/hijack";
}

string cmakex_config_t::default_deps_source_dir() const
{
    return cmake_binary_dir + "/_deps";
}

string cmakex_config_t::default_deps_build_dir() const
{
    return cmake_binary_dir + "/_deps-build";
}

string cmakex_config_t::default_deps_install_dir() const
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
            badpars_exit(stringf("Source path %s is a directory but contains no 'CMakeLists.txt'.",
                                 path_for_log(x).c_str()));
    } else if (allow_invalid)
        return false;
    else
        badpars_exit(stringf("Source path not found: %s.", path_for_log(x).c_str()));

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

string format_cmake_arg(const parsed_cmake_arg_t& a)
{
    if (is_one_of(a.switch_, {"-C", "-G", "-T", "-A"})) {
        CHECK(a.type.empty() && a.name.empty());
        return stringf("%s%s", a.switch_.c_str(), a.value.c_str());
    }
    if (a.switch_ == "-U") {
        CHECK(a.type.empty());
        return stringf("-U%s", a.name.c_str());
    }
    if (a.switch_ == "-D") {
        if (a.type.empty())
            return stringf("-D%s=%s", a.name.c_str(), a.value.c_str());
        else
            return stringf("-D%s:%s=%s", a.name.c_str(), a.type.c_str(), a.value.c_str());
    }

    if (a.switch_ == "--graphwiz==") {
        CHECK(a.type.empty() && a.name.empty());
        return stringf("--graphiz==%s", a.value.c_str());
    }

    CHECK(a.name.empty() && a.type.empty() && a.value.empty());
    return a.switch_;
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

pkg_request_t pkg_request_from_args(const vector<string>& pkg_args,
                                    const vector<config_name_t>& default_configs)
{
    CHECK(!default_configs.empty());
    if (pkg_args.empty())
        throwf("Empty package descriptor, package name is missing.");
    const string request_name = pkg_args[0];
    const auto args = parse_arguments(
        {"DEFINE_ONLY"},
        {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "GIT_TAG_OVERRIDE", "SOURCE_DIR", "GIT_SHALLOW"},
        {"DEPENDS", "CMAKE_ARGS", "CONFIGS"}, vector<string>(pkg_args.begin() + 1, pkg_args.end()));

    bool define_only = args.count("DEFINE_ONLY");

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
    if (!define_only && configs.empty()) {
        configs = default_configs;
        using_default_configs = true;
    }

    pkg_request_t request(pkg_args[0], stable_unique(configs), using_default_configs);

    request.define_only = define_only;

    for (auto c : {"GIT_REPOSITORY", "GIT_URL", "GIT_TAG", "GIT_TAG_OVERRIDE", "SOURCE_DIR"}) {
        auto count = args.count(c);
        CHECK(count == 0 || args.at(c).size() == 1);
        if (count > 0 && args.at(c).empty())
            throwf("Empty string after '%s'.", c);
    }
    string a, b;

    if (args.count("GIT_REPOSITORY") > 0) {
        if (args.count("GIT_URL") > 0)
            throwf("Both GIT_URL and GIT_REPOSITORY are specified.");
        request.c.git_url = args.at("GIT_REPOSITORY")[0];
    } else if (args.count("GIT_URL") > 0)
        request.c.git_url = args.at("GIT_URL")[0];

    if (args.count("GIT_TAG") > 0) {
        if (args.count("GIT_TAG_OVERRIDE") > 0)
            throwf("Both GIT_TAG and GIT_TAG_IGNORE are specified.");
        request.c.git_tag = args.at("GIT_TAG")[0];
    } else if (args.count("GIT_TAG_OVERRIDE") > 0) {
        request.c.git_tag = args.at("GIT_TAG_OVERRIDE")[0];
        request.git_tag_override = true;
    }

    if (args.count("GIT_SHALLOW") > 0)
        request.git_shallow = eval_cmake_boolean_or_fail(args.at("GIT_SHALLOW")[0]);
    if (args.count("SOURCE_DIR") > 0) {
        request.b.source_dir = args.at("SOURCE_DIR")[0];
        if (fs::path(request.b.source_dir).is_absolute())
            throwf("SOURCE_DIR must be a relative path: %s",
                   path_for_log(request.b.source_dir).c_str());
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
    std::map<string, string> varname_to_last_arg;  // maps variable name to the last option that
                                                   // deals with that variable
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
                    "Invalid CMAKE_ARG: '%s', globbing characters '*' and '?' are not "
                    "supported "
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
                        pca.switch_ = "-G";
                        break;
                    case 1:
                        a = "-T" + pca.value;
                        pca.switch_ = "-T";
                        break;
                    case 2:
                        a = "-A" + pca.value;
                        pca.switch_ = "-A";
                        break;
                    default:
                        CHECK(gta_idx == 3);
                }
            }
        }  // if -U or -D

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
                            "Two, different '%s' options has been specified: '%s' "
                            "and '%s'. "
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
    }  // for all args
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

tuple<vector<string>, bool> cmake_args_prepend_cmake_path_variable(vector<string> cmake_args,
                                                                   string_par var_name,
                                                                   string_par dir)
{
    for (auto& c : cmake_args) {
        auto pca = parse_cmake_arg(c);
        if (pca.switch_ == "-D" && pca.name == var_name) {
            prepend_inplace(pca.value, dir.str() + ";");
            c = format_cmake_arg(pca);
            return make_tuple(move(cmake_args), true);
        }
    }
    return make_tuple(move(cmake_args), false);
}

vector<string> cmakex_prefix_path_to_vector(string_par x, bool env_var)
{
    vector<string> r;
    auto v = split(x, env_var ? system_path_separator() : ';');
    for (auto& p : v) {
        if (!p.empty())
            r.emplace_back(fs::lexically_normal(p).string());
    }
    return r;
}

cmake_cache_t read_cmake_cache(string_par path)
{
    const vector<string> words = {"CMAKE_HOME_DIRECTORY",
                                  "CMAKE_GENERATOR",
                                  "CMAKE_GENERATOR_TOOLSET",
                                  "CMAKE_GENERATOR_PLATFORM",
                                  "CMAKE_EXTRA_GENERATOR",
                                  "CMAKE_PREFIX_PATH",
                                  "CMAKE_ROOT",
                                  "CMAKE_MODULE_PATH",
                                  "CMAKE_BUILD_TYPE"};
    cmake_cache_t cache;
    auto f = must_fopen(path, "r");
    while (!feof(f)) {
        auto line = must_fgetline_if_not_eof(f);
        for (auto s : words) {
            if (starts_with(line, stringf("%s:", s.c_str())) ||
                starts_with(line, stringf("%s=", s.c_str()))) {
                auto colon_pos = line.find(':');
                auto equal_pos = line.find('=');
                if (equal_pos != string::npos) {
                    if (colon_pos < equal_pos)
                        cache.types[s] = line.substr(colon_pos + 1, equal_pos - colon_pos - 1);
                    cache.vars[s] = line.substr(equal_pos + 1);
                    break;
                }
            }
        }
        if (cache.vars.size() == words.size())
            break;
    }
    return cache;
}
void write_hijack_module(string_par pkg_name, string_par binary_dir)
{
    cmakex_config_t cfg(binary_dir);
    string dir = cfg.find_module_hijack_dir();

    if (!fs::is_directory(dir))
        fs::create_directories(dir);
    static const char* const c_fptcf_filename = "FindPackageTryConfigFirst.cmake";
    string file = dir + "/" + c_fptcf_filename;
    if (!fs::is_regular_file(file))
        must_write_text(file, k_find_package_try_config_first_module_content);
    file = dir + "/Find" + pkg_name.str() + ".cmake";

    const char* generating_or_using = "Using";
    if (!fs::is_regular_file(file)) {
        generating_or_using = "Generating";
        must_write_text(file,
                        "include(FindPackageTryConfigFirst)\nfind_package_try_config_first()\n");
    }

    log_info(
        "%s a special 'Find%s.cmake' which diverts "
        "'find_package(%s ...)' from finding the official find-module and "
        "finds "
        "the the config-module instead. This hijacker find-module is "
        "written to %s which is automatically added to all projects' "
        "CMAKE_MODULE_PATHs.",
        generating_or_using, pkg_name.c_str(), pkg_name.c_str(), path_for_log(dir).c_str());
}

const string* find_specific_cmake_arg_or_null(string_par cmake_var_name,
                                              const vector<string>& cmake_args)
{
    for (auto& arg : cmake_args) {
        auto pca = parse_cmake_arg(arg);
        if (pca.name == cmake_var_name)
            return &arg;
    }
    return nullptr;
}

vector<string> make_sure_cmake_path_var_contains_path(
    string_par bin_dir,
    string_par var_name,     // like "CMAKE_PREFIX_PATH"
    string_par path_to_add,  // like install dir of the dependencies
    vector<string> cmake_args)
{
    string cmake_path_var_value;
    string cmake_path_var_type;

    // make sure CMAKE_PREFIX_PATH and CMAKE_MODULE_PATH variables contains our special paths
    do {  // scope for break
        if (!fs::is_directory(path_to_add.c_str()))
            break;
        bool b;
        // try to prepend the the cmake path arg (if it exists) with path_to_add
        tie(cmake_args, b) =
            cmake_args_prepend_cmake_path_variable(cmake_args, var_name, path_to_add);
        if (b)
            break;  // current cmake_args contains the variable in question and we prepended
                    // it
        cmake_path_var_value = path_to_add.str();
        auto cmake_cache_path = bin_dir.str() + "/CMakeCache.txt";
        if (!fs::is_regular_file(cmake_cache_path))
            break;  // no CMakeCache.txt (initial build), add dir as the only value of this
                    // path
                    // variable
        auto cmake_cache = read_cmake_cache(cmake_cache_path);
        auto it = cmake_cache.vars.find(var_name.c_str());
        auto itt = cmake_cache.types.find(var_name.c_str());
        if (itt != cmake_cache.types.end())
            cmake_path_var_type = itt->second;
        if (it == cmake_cache.vars.end() || it->second.empty())
            break;  // the cmake path variable was set in cache, add dir as the only value
                    // of
                    // this path variable
        auto dirs = split(it->second, ';');
        for (auto& d : dirs) {
            if (fs::is_directory(d) && fs::equivalent(d, path_to_add.str())) {
                cmake_path_var_value.clear();  // don't add if it already contains it
                break;
            }
        }
        if (!cmake_path_var_value.empty())
            cmake_path_var_value += ";" + it->second;  // prepend existing
    } while (false);

    if (!cmake_path_var_value.empty()) {
        cmake_args.emplace_back(
            stringf("-D%s%s=%s", var_name.c_str(),
                    cmake_path_var_type.empty() ? "" : (":" + cmake_path_var_type).c_str(),
                    cmake_path_var_value.c_str()));
    }
    return cmake_args;
}

string escape_cmake_arg(string_par x)
{
    bool quote = false;
    string result;
    result.reserve(x.size());
    for (auto c : x) {
        if (!isalnum(c) && c != '_' && c != '-' && c != '+' && c != '=' && c != '/' && c != '.')
            quote = true;
        switch (c) {
            case '$':
            case '"':
            case '\\':
            case ';':
                result += stringf("\\%c", c);
                break;
            default:
                result += c;
        }
    }
    if (quote)
        result = stringf("\"%s\"", result.c_str());
    return result;
}
string escape_command_line_arg(string_par x)
{
    bool quote = false;
    string result;
    result.reserve(x.size());
    for (auto c : x) {
        if (!isalnum(c) && c != '_' && c != '/' && c != '-' && c != '=' && c != '.' && c != ',')
            quote = true;
        switch (c) {
            case '"':
            case '\\':
            case '!':
                result += stringf("\\%c", c);
                break;
            default:
                result += c;
        }
    }
    if (quote)
        result = stringf("\"%s\"", result.c_str());
    return result;
}
}
