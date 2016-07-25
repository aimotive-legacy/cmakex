#include "run_add_pkgs.h"

#include <map>
#include <regex>

#include <adasworks/sx/check.h>

#include "circular_dependency_detector.h"
#include "clone.h"
#include "cmakex-types.h"
#include "cmakex_utils.h"
#include "filesystem.h"
#include "git.h"
#include "install_deps_phase_one.h"
#include "installdb.h"
#include "misc_utils.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;

using pkg_request_map_t = std::map<string, pkg_request_t>;

using pkg_processing_statuses = std::map<string, pkg_request_eval_details_t>;

void add_pkg_after_clone(string_par pkg_name,
                         const cmakex_pars_t& pars,
                         pkg_request_map_t& pkg_requests,
                         pkg_processing_statuses& pkg_statuses)
{
    CHECK(false);
}

void add_pkg(const string& pkg_name,
             const string& pkg_that_needs_it,
             const cmakex_pars_t& pars,
             pkg_request_map_t& pkg_requests,
             pkg_processing_statuses& pkg_statuses)
{
    if (pkg_statuses.count(pkg_name) > 0)
        return;  // already delt with

    log_info("Adding %s", pkg_for_log(pkg_name).c_str());

    auto& status = pkg_statuses[pkg_name];

    InstallDB installdb(pars.binary_dir);
    circular_dependency_detector cdd(pars.binary_dir);
    if (cdd.contains(pkg_name)) {
        // that means circular dependency
        CHECK(!pkg_that_needs_it.empty());
        auto dep_line = cdd.get_stack_since(pkg_name);
        string s;
        for (auto& d : dep_line) {
            if (!s.empty())
                s += " -> ";
            s += d;
        }
        throwf("Circular dependency detected ('->' means 'needs'): %s", s.c_str());
    }
    cdd.push(pkg_name);
    try {
        auto& req = pkg_requests.at(pkg_name);
        // we need to satisfy the requirements of 'req'
        // - it may be satisfied by the current state of the install directory, that is
        //   it is already installed -> nothing to do
        // - it may be partly satisfied (e.g. Debug & Release configs are requested
        //   but we have only Release installed -> build the missing config
        // - it may be installed but contradicts with the request -> fail
        // - it may no be installed -> clone + satisfy dependencies than add it to the
        //   list of packages to be built + installed

        status = installdb.evaluate_pkg_request_build_pars(req.name, req.b);
        bool do_build = false;
        cmakex_config_t cfg(pars.binary_dir);

        switch (status.status) {
            case pkg_request_satisfied:
                log_info("Package %s already installed", pkg_name.c_str());
                break;
            case pkg_request_missing_configs:
                // - make sure the package is cloned out exactly with clone-related options it was
                // installed
                {
                    auto cp = req.c;
                    cp.git_tag = installdb.try_get_installed_pkg_desc(pkg_name)->c.git_tag;
                    make_sure_exactly_this_sha_is_cloned_or_fail(pkg_name, cp, req.git_shallow,
                                                                 pars.binary_dir);
                    do_build = true;
                }
                break;
            case pkg_request_not_installed:
                // - make sure the package is cloned out: there are two modes of operation:
                //   - strict (default) build only exactly the required clone
                //   - permissive: build whatever is there, issue warning
                make_sure_exactly_this_git_tag_is_cloned(pkg_name, req.c, req.git_shallow,
                                                         pars.binary_dir, pars.strict_commits);
                do_build = true;
                break;
            case pkg_request_not_compatible:
                throwf(
                    "Package request for %s cannot be satisfied due to incompatible build "
                    "options. Remove the package manually to allow it build with the new options "
                    "or remove the conflicting options. The conflicting options: %s",
                    pkg_for_log(pkg_name).c_str(), status.incompatible_cmake_args.c_str());
            default:
                CHECK(false);
        }

        if (do_build) {
            // find out if the build target is a dir containing CMakeList.txt or a build script
            string clone_dir = cfg.pkg_clone_dir(pkg_name);
            string d = clone_dir;
            bool build_script_valid = false;
            string build_script_path;
            if (req.b.source_dir.empty()) {
                string cmakelists_path = clone_dir + "/CMakeLists.txt";
                if (fs::is_regular_file(cmakelists_path))
                    build_script_valid = true;
                else
                    throwf(
                        "The root of the repository does not contain 'CMakeLists.txt' (no "
                        "SOURCE_DIR specified).");
            } else {
                string d = clone_dir + "/" + req.b.source_dir;
                if (fs::is_directory(d)) {
                    string cmakelists_path = d + "/CMakeLists.txt";
                    if (fs::is_regular_file(d))
                        build_script_valid = true;
                    else
                        throwf(
                            "The directory specified by the SOURCE_DIR option does not contain "
                            "'CMakeLists.txt' (\"%s\")",
                            d.c_str());
                } else
                    throwf("The path specified by the SOURCE_DIR option is not a directory (\"%s\"",
                           d.c_str());
            }
            CHECK(build_script_valid);
            add_pkg_after_clone(pkg_name, pars, pkg_requests, pkg_statuses);
        }
    } catch (...) {
        cdd.pop(pkg_name);
        throw;
    }

    cdd.pop(pkg_name);
}

void run_add_pkgs(const cmakex_pars_t& pars)
{
    CHECK(!pars.add_pkgs.empty());
    CHECK(pars.b.source_dir.empty());

    pkg_request_map_t pkg_requests;

    for (auto& pkg_arg_str : pars.add_pkgs) {
        auto request = pkg_request_from_arg_str(pkg_arg_str);
        if (pkg_requests.count(request.name) > 0)
            throwf("Package '%s' is duplicated.", request.name.c_str());
        pkg_requests[request.name] = move(request);
    }

    // First phase: evaluate each package whether it is already installed, should be build or
    // incompatible.
    // Each package that it not installed  will be cloned out to resolve the exact dependencies and
    // build options.
    // Each package that is partly installed (some configurations are missing) will be cloned out to
    // build the missing configurations.
    pkg_processing_statuses pkg_statuses;
    for (auto& kv : pkg_requests)
        add_pkg(kv.first, "", pars, pkg_requests, pkg_statuses);
}

#if 0
void f()
{
    deps_to_process.emplace_back(kv.first);

    // collect all dependencies (those not listed in pkg_requests)

    // deps_to_process: new dependencies that are to be found in pkg_requests but whose dependencies
    // has not been checked
    vector<string> deps_to_process;

    // initialize with current requests
    for (auto& kv : pkg_requests)
        deps_to_process.emplace_back(kv.first);

    // new_deps_found: new packages found in dependencies of packages listed in deps_to_process
    vector<string> new_deps_found;

    for (auto& d : deps_to_process) {
        for (auto& dd : pkg_requests[d].depends) {
            if (pkg_requests.count(dd) == 0)
                new_deps_found.emplace_back(dd);
        }
    }

    // packages listed in new_deps_found must be
    // - already installed in installdb so we have package definitions string there
    // - available from registry

    InstallDB installdb;

    for (auto& d : new_deps_found) {
        auto maybe_def = installdb.try_get_pkg_def(d);
        if (!maybe_def)
            maybe_def = registry.try_get_pkg_def(d);
        if (!maybe_def)
            throwf("Dependency %s is not installed and not found in registry.", pkg_for_log(d).c_str());
        pkg_requests[d] = *maybe_def;
    }

    for (auto& kv : pkg_requests) {
    }
    // determine build order from dependency graph
    for (auto& kv : pkg_requests) {
        auto& req = kv.second;
        for (auto& d : req.depends) {
            if (pkg_request_t.count(d) == 0)  // depends on a package not listed here
        }
    }
}
#endif
#if 0
void add_pkg(pkg_requests_t& pkg_requests, pair<string, pkg_request_t>& kv)
{
    // make sure all dependecies are installed
}
#endif
#if 0
void add_pkg(const cmakex_pars_t& pars, const string& pkg_arg_str)
{
    auto request = pkg_request_from_args(pkg_arg_str);

    log_info("Adding %s", pkg_for_log(request.name).c_str());

    // Add 'Release' if no configs specified
    if (request.configs.empty())
        request.configs = {"Release"};

    const cmakex_config_t cfg(pars.binary_dir);

    const string install_prefix = cfg.cmakex_deps_install_prefix + "/" + request.name;
    const string clone_dir = cfg.cmakex_deps_clone_prefix + "/" + request.name;

    for (auto config : request.configs) {
        // create
    }
}
#endif

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
        for (auto& a : args["CMAKE_ARGS"]) {
            if (!request.b.cmake_args.empty() &&
                is_one_of(request.b.cmake_args.back(), {"-C", "-D", "-U", "-G", "-T", "-A"})) {
                request.b.cmake_args.back() += a;
            } else
                request.b.cmake_args.emplace_back(a);
        }
        request.b.cmake_args = args["CMAKE_ARGS"];
        for (auto& a : request.b.cmake_args) {
            if (starts_with(a, "-D")) {
                int i = 2;
                while (i < a.size() && a[i] != '=' && a[i] != ':')
                    ++i;
                string variable = a.substr(2, i - 2);
                if (is_one_of(variable,
                              {"CMAKE_INSTALL_PREFIX", "CMAKE_PREFIX_PATH", "CMAKE_MODULE_PATH"}))
                    throwf("Setting '%s' is not allowed in CMAKE_ARGS", variable.c_str());
            }
        }
    }
    if (args.count("CONFIGS") > 0)
        request.b.configs = args["CONFIGS"];

    return request;
}

#if 0
cmakex_pars_t ppars;
ppars.c = true;
ppars.b = true;
ppars.t = pars.t;
ppars.configs = request.configs.empty() ? pars.configs : request.configs;
ppars.binary_dir = cfg.cmakex_deps_binary_prefix + "/" + request.name;
ppars.source_desc = cfg.cmakex_deps_clone_prefix + "/" + request.name;
if (!request.source_dir.empty())
    ppars.source_desc += "/" + request.source_dir;
ppars.config_args = pars.config_args;
ppars.config_args.insert(ppars.config_args.end(), BEGINEND(request.cmake_args));
ppars.build_args = pars.build_args;
ppars.native_tool_args = pars.native_tool_args;
ppars.build_targets = {"install"};
ppars.config_args_besides_binary_dir = true;

// check if no install prefix in configs
for (auto s : ppars.config_args) {
    if (starts_with(s, "-DCMAKE_INSTALL_PREFIX:") || starts_with(s, "-DCMAKE_INSTALL_PREFIX="))
        throwf("In '-P' mode 'CMAKE_INSTALL_PREFIX' can't be set manually.");
}

ppars.config_args.emplace_back(string("-DCMAKE_INSTALL_PREFIX:PATH=") + request.install_prefix);

// clone
// remove clone dir if empty
if (fs::is_directory(request.clone_dir)) {
    try {
        fs::remove(request.clone_dir);
    } catch (...) {
    }
}
// clone if not exists
if (fs::is_directory(request.clone_dir)) {
    // attempt to resolve request.git_tag to SHA with git ls-remote
    string ref = request.git_tag.empty() ? "HEAD" : request.git_tag;
    int result;
    string remote_sha;
    tie(result, remote_sha) = git_ls_remote(request.git_url, ref);
    const std::regex c_regex_sha("^[0-9a-fA-F]+$");
    if (result == 2 && regex_match(request.git_tag, c_regex_sha)) {
        LOG_DEBUG("git ls-remote returned 2, GIT_URL is like an SHA");
        remote_sha = "request-is-sha";
        result = 0;
    } else if (result != 0) {
        LOG_DEBUG("git ls-remote failed");
    } else if (std::regex_match(remote_sha, c_regex_sha)) {
        log_info("GIT_TAG=%s resolved to %s", ref.c_str(), remote_sha.c_str());
    } else {
        LOG_DEBUG("Output of 'git ls-remote': \"%s\" is not like an SHA.", remote_sha.c_str());
        result = 1;
    }
}
vector<string> args = {"clone"};
//   if (!cfg.full_clone)

//     execute_git({"clone", "--depth 1", "--branch", "--recursive"});
}

ppars.source_desc_kind = evaluate_source_descriptor(ppars.source_desc);
}
#endif
}
