#include "install_deps_phase_one.h"

#include <adasworks/sx/algorithm.h>
#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

#include "clone.h"
#include "cmakex_utils.h"
#include "exec_process.h"
#include "filesystem.h"
#include "git.h"
#include "helper_cmake_project.h"
#include "installdb.h"
#include "misc_utils.h"
#include "print.h"
#include "run_add_pkgs.h"

namespace cmakex {

namespace fs = filesystem;

void fail_if_current_clone_has_different_commit(string req_git_tag,
    string_par clone_dir,
    string_par cloned_sha,
    string_par git_url)
{
    // if HEAD/branch is requested need to check ls-remote
    // if tag/SHA requested check local clone

    if (req_git_tag.empty())
        req_git_tag = "HEAD";

    string msg = stringf(
        "Because of the '--strict' option the directory \"%s\" should be "
        "reset to the remote's '%s' commit in order to build it. Reset manually or remove the "
        "directory.",
        clone_dir.c_str(), req_git_tag.c_str());

    if (cloned_sha == k_sha_uncommitted) // no need to check further
        throwf("%s", msg.c_str());
    // find out if we have the exact same commit cloned out as req_git_tag
    // req_git_tag can be empty=branch/tag/sha
    if (cloned_sha != req_git_tag) { // req_git_tag was an SHA
        // check remote
        auto lsr = git_ls_remote(git_url, req_git_tag);
        if (get<0>(lsr) == 0) {
            if (cloned_sha != get<1>(lsr))
                throwf("%s", msg.c_str());
        }
        else {
            throwf(
                "Because of the '--strict' option the requested ref '%s' "
                "needs to be resolved by the remote \"%s\" but 'git "
                "ls-remote' failed (%d)",
                req_git_tag.c_str(), git_url.c_str(), get<0>(lsr));
        }
    }
}

vector<string> install_deps_phase_one_deps_script(string_par binary_dir,
    string_par deps_script_filename,
    const vector<string>& global_cmake_args,
    const vector<config_name_t>& configs,
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache);

vector<string> install_deps_phase_one_request_deps(string_par binary_dir,
    vector<string> request_deps,
    const vector<string>& global_cmake_args,
    const vector<config_name_t>& configs,
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache)
{
    vector<string> pkgs_encountered;

    // for each pkg:
    for (auto& d : request_deps) {
        auto pkgs_encountered_below = run_deps_add_pkg({ { d } }, binary_dir, global_cmake_args, configs, wsp, cmakex_cache);
        pkgs_encountered.insert(pkgs_encountered.end(), BEGINEND(pkgs_encountered_below));
    }

    return pkgs_encountered;
}

vector<string> install_deps_phase_one(string_par binary_dir,
    string_par source_dir,
    vector<string> request_deps,
    const vector<string>& global_cmake_args,
    const vector<config_name_t>& configs,
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache)
{
    CHECK(!binary_dir.empty());
    CHECK(!configs.empty());
    if (!source_dir.empty()) {
        string deps_script_file = fs::lexically_normal(fs::absolute(source_dir.str()).string() + "/" + k_deps_script_filename);
        if (fs::is_regular_file(deps_script_file))
            return install_deps_phase_one_deps_script(
                binary_dir, deps_script_file, global_cmake_args, configs, wsp, cmakex_cache);
    }
    return install_deps_phase_one_request_deps(binary_dir, request_deps, global_cmake_args, configs,
        wsp, cmakex_cache);
}

vector<string> install_deps_phase_one_deps_script(string_par binary_dir_sp,
    string_par deps_script_file,
    const vector<string>& global_cmake_args,
    const vector<config_name_t>& configs,
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache)
{
    // Create helper cmake project
    // Configure it again with specifying the build script as parameter
    // The background project executes the build script and
    // - records the add_pkg commands
    // - records the cmakex commands
    // Then the configure ends.
    // Process the recorded add_pkg commands which installs the requested dependency to the install directory.

    // create source dir

    log_info("Configuring \"%s\" (using script executor helper project)", deps_script_file.c_str());
    HelperCmakeProject hcp(binary_dir_sp);
    hcp.configure(global_cmake_args, cmakex_cache);
    auto addpkgs_lines = hcp.run_deps_script(deps_script_file);

    vector<string> pkgs_encountered;
    string binary_dir = fs::absolute(binary_dir_sp.c_str()).string();

    // for each pkg:
    for (auto& addpkg_line : addpkgs_lines) {
        auto pkgs_encountered_below = run_deps_add_pkg(
            split(addpkg_line, '\t'), binary_dir, global_cmake_args, configs, wsp, cmakex_cache);
        pkgs_encountered.insert(pkgs_encountered.end(), BEGINEND(pkgs_encountered_below));
    }

    return pkgs_encountered;
}

vector<string> run_deps_add_pkg(const vector<string>& args,
    string_par binary_dir,
    const vector<string>& global_cmake_args,
    const vector<config_name_t>& configs,
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache)
{
    const cmakex_config_t cfg(binary_dir);

    auto pkg_request = pkg_request_from_args(args);

    // add global cmake args
    append(pkg_request.b.cmake_args, global_cmake_args);
    pkg_request.b.cmake_args = normalize_cmake_args(pkg_request.b.cmake_args);

    if (std::find(BEGINEND(wsp.requester_stack), pkg_request.name) != wsp.requester_stack.end()) {
        string s;
        for (auto& x : wsp.requester_stack) {
            if (!s.empty())
                s += " -> ";
            s += x;
        }
        s += "- > ";
        s += pkg_request.name;
        throwf("Circular dependency: %s", s.c_str());
    }

    // if it's already installed we still need to process this:
    // - to enumerate all dependencies
    // - to check if only compatible installations are requested

    // todo: get data from optional package registry here

    // fill configs from global par if not specified
    if (pkg_request.b.configs.empty())
        pkg_request.b.configs = configs;

    pkg_request.b.configs = stable_unique(pkg_request.b.configs);

    // it's already processed
    // 1. we may add a new configs to the planned ones
    // 2. must check if other args are compatible
    const bool already_processed = wsp.pkg_map.count(pkg_request.name) > 0;
    if (already_processed) {
        auto& pd = wsp.pkg_map[pkg_request.name].planned_desc;
        // compare SOURCE_DIR
        auto& s1 = pd.b.source_dir;
        auto& s2 = pkg_request.b.source_dir;
        if (s1 != s2) {
            throwf(
                "Different SOURCE_DIR args for the same package. The package '%s' is being added "
                "for the second time. The first time the SOURCE_DIR was \"%s\" and not it's "
                "\"%s\".",
                pkg_request.name.c_str(), s1.c_str(), s2.c_str());
        }
        // compare CMAKE_ARGS
        auto v = incompatible_cmake_args(pd.b.cmake_args, pkg_request.b.cmake_args);
        if (!v.empty()) {
            throwf(
                "Different CMAKE_ARGS args for the same package. The package '%s' is being added "
                "for the second time but the following CMAKE_ARGS are incompatible with the first "
                "request: %s",
                pkg_request.name.c_str(), join(v, ", ").c_str());
        }
    }

    clone_helper_t clone_helper(binary_dir, pkg_request.name);
    auto& cloned = clone_helper.cloned;
    auto& cloned_sha = clone_helper.cloned_sha;

    auto clone_this = [&pkg_request, &wsp, &clone_helper] {
        clone_helper.clone(pkg_request.c, pkg_request.git_shallow);
        wsp.pkg_map[pkg_request.name].just_cloned = true;
    };

    string pkg_source_dir = cfg.pkg_clone_dir(pkg_request.name);
    if (!pkg_request.b.source_dir.empty())
        pkg_source_dir += "/" + pkg_request.b.source_dir;

    // determine installed status
    InstallDB installdb(binary_dir);
    auto installed_result = installdb.evaluate_pkg_request_build_pars(pkg_request.name, pkg_request.b);
    string clone_dir = cfg.pkg_clone_dir(pkg_request.name);
    bool must_build = false;
    for (auto& kv : installed_result) {
        switch (kv.second.status) {
        case pkg_request_not_installed:
        case pkg_request_not_compatible:
            must_build = true;
            break;
        case pkg_request_satisfied:
            //todo still needs to build if there's new commit in cloned repo or dependency changed
            break;
        default:
            CHECK(false);
        }
        if (must_build)
            break;
    }

    if (must_build && !cloned)
        clone_this();

    string unresolved_git_tag = pkg_request.c.git_tag;

    //todo: rather we need to remember if we're keeping the current install, because we it's installed at the requested commit, or we'll build it
    // if we're about to build we need to clone which will have a single cloned_sha
    // if we're not going to build it, there may be different commits installed
    if (cloned) {
        pkg_request.c.git_tag = cloned_sha;
    }

    if (already_processed) {
        // verify final, resolved commit SHAs
        auto& pm = wsp.pkg_map[pkg_request.name];
        string prev_git_tag = pm.planned_desc.c.git_tag;
        if (pkg_request.c.git_tag != prev_git_tag) {
            throwf(
                "Different GIT_TAG's: this package has already been requested but with different "
                "GIT_TAG specification. Previous GIT_TAG was '%s', resolved as %s, current GIT_TAG "
                "is '%s', resolved as %s",
                pm.unresolved_git_tag.c_str(), prev_git_tag.c_str(), unresolved_git_tag.c_str(),
                pkg_request.c.git_tag.c_str());
        }
    }

    vector<string> pkgs_encountered;

    //todo: we need to return from install_deps_phase_one if we're going to build a dependency which should trigger build here, too

    //note: for now, since a dependency with uncommitteed changes will always be built, all the packages that depend on it will be rebuilt

    // process_deps
    {
        if (cloned) {
            wsp.requester_stack.emplace_back(pkg_request.name);

            pkgs_encountered = install_deps_phase_one(binary_dir, pkg_source_dir, keys_of_map(pkg_request.deps_shas),
                global_cmake_args, configs, wsp, cmakex_cache);

            CHECK(wsp.requester_stack.back() == pkg_request.name);
            wsp.requester_stack.pop_back();
        }
        else {
            // enumerate dependencies from description of installed package
            for (auto& kv : installed_result) {
                CHECK(kv.second.status == pkg_request_satisfied);
                auto deps = keys_of_map(kv.second.installed_config_desc.deps_shas);
                pkgs_encountered.insert(pkgs_encountered.end(), BEGINEND(deps));
            }
        }
    }

    std::sort(BEGINEND(pkgs_encountered));
    sx::unique_trunc(pkgs_encountered);

    auto& pm = wsp.pkg_map[pkg_request.name];
    auto& pd = pm.planned_desc;
    if (already_processed) {

        //todo: we install now per config. think over what will happen here

        // extend configs if needed
        pd.b.configs.insert(pd.b.configs.end(), BEGINEND(pkg_request.b.configs));
        std::sort(BEGINEND(pd.b.configs));
        sx::unique_trunc(pd.b.configs);
        // extend depends if needed
        for (auto& d : pkgs_encountered) {
            if (pd.deps_shas.count(d) == 0)
                pd.deps_shas[d]; //insert empty string
        }
    }
    else {
        wsp.build_order.push_back(pkg_request.name);
        pm.unresolved_git_tag = unresolved_git_tag;
        pd = pkg_request;
        for (auto& d : pkgs_encountered)
            pd.deps_shas[d]; //insert empty string
    }
    pkgs_encountered.emplace_back(pkg_request.name);
    return pkgs_encountered;
}
}
