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

void idpo_recursion_result_t::add(const idpo_recursion_result_t& x)
{
    append(pkgs_encountered, x.pkgs_encountered);
    building_some_pkg |= x.building_some_pkg;
    normalize();
}

void idpo_recursion_result_t::normalize()
{
    std::sort(BEGINEND(pkgs_encountered));
    sx::unique_trunc(pkgs_encountered);
}

void idpo_recursion_result_t::add_pkg(string_par x)
{
    pkgs_encountered.emplace_back(x.str());
    normalize();
}

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

    if (cloned_sha == k_sha_uncommitted)  // no need to check further
        throwf("%s", msg.c_str());
    // find out if we have the exact same commit cloned out as req_git_tag
    // req_git_tag can be empty=branch/tag/sha
    if (cloned_sha != req_git_tag) {  // req_git_tag was an SHA
        // check remote
        auto lsr = git_ls_remote(git_url, req_git_tag);
        if (get<0>(lsr) == 0) {
            if (cloned_sha != get<1>(lsr))
                throwf("%s", msg.c_str());
        } else {
            throwf(
                "Because of the '--strict' option the requested ref '%s' "
                "needs to be resolved by the remote \"%s\" but 'git "
                "ls-remote' failed (%d)",
                req_git_tag.c_str(), git_url.c_str(), get<0>(lsr));
        }
    }
}

idpo_recursion_result_t install_deps_phase_one_deps_script(string_par binary_dir,
                                                           string_par deps_script_filename,
                                                           const vector<string>& global_cmake_args,
                                                           const vector<config_name_t>& configs,
                                                           deps_recursion_wsp_t& wsp,
                                                           const cmakex_cache_t& cmakex_cache);

idpo_recursion_result_t install_deps_phase_one_request_deps(string_par binary_dir,
                                                            vector<string> request_deps,
                                                            const vector<string>& global_cmake_args,
                                                            const vector<config_name_t>& configs,
                                                            deps_recursion_wsp_t& wsp,
                                                            const cmakex_cache_t& cmakex_cache)
{
    idpo_recursion_result_t rr;

    // for each pkg:
    for (auto& d : request_deps) {
        auto rr_below =
            run_deps_add_pkg({{d}}, binary_dir, global_cmake_args, configs, wsp, cmakex_cache);
        rr.add(rr_below);
    }

    return rr;
}

idpo_recursion_result_t install_deps_phase_one(string_par binary_dir,
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
        string deps_script_file = fs::lexically_normal(fs::absolute(source_dir.str()).string() +
                                                       "/" + k_deps_script_filename);
        if (fs::is_regular_file(deps_script_file))
            return install_deps_phase_one_deps_script(
                binary_dir, deps_script_file, global_cmake_args, configs, wsp, cmakex_cache);
    }
    return install_deps_phase_one_request_deps(binary_dir, request_deps, global_cmake_args, configs,
                                               wsp, cmakex_cache);
}

idpo_recursion_result_t install_deps_phase_one_deps_script(string_par binary_dir_sp,
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
    // Process the recorded add_pkg commands which installs the requested dependency to the install
    // directory.

    // create source dir

    log_info("Configuring \"%s\" (using script executor helper project)", deps_script_file.c_str());
    HelperCmakeProject hcp(binary_dir_sp);
    hcp.configure(global_cmake_args, cmakex_cache);
    auto addpkgs_lines = hcp.run_deps_script(deps_script_file);

    idpo_recursion_result_t rr;
    string binary_dir = fs::absolute(binary_dir_sp.c_str()).string();

    // for each pkg:
    for (auto& addpkg_line : addpkgs_lines) {
        auto rr_below = run_deps_add_pkg(split(addpkg_line, '\t'), binary_dir, global_cmake_args,
                                         configs, wsp, cmakex_cache);
        rr.add(rr_below);
    }

    return rr;
}

idpo_recursion_result_t run_deps_add_pkg(const vector<string>& args,
                                         string_par binary_dir,
                                         const vector<string>& global_cmake_args,
                                         const vector<config_name_t>& configs,
                                         deps_recursion_wsp_t& wsp,
                                         const cmakex_cache_t& cmakex_cache)
{
    const cmakex_config_t cfg(binary_dir);

    auto pkg_request = pkg_request_from_args(args);

    // add global cmake args
    prepend(pkg_request.b.cmake_args, global_cmake_args);
    pkg_request.b.cmake_args = normalize_cmake_args(pkg_request.b.cmake_args);

    if (linear_search(wsp.requester_stack, pkg_request.name)) {
        // report circular dependency error
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

    // it's already processed we need to check if it's the same request as before
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

        // compare configs
        if (pkg_request.b.configs != pd.b.configs) {
            auto v1 = join(get_prefer_NoConfig(pd.b.configs), ", ");
            auto v2 = join(get_prefer_NoConfig(pkg_request.b.configs), ", ");
            throwf(
                "Different configurations requested for the same package. Previously, for package "
                "'%s' these configurations had been requested: (%s) and now these: (%s)",
                pkg_request.name.c_str(), v1.c_str(), v2.c_str());
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
    auto installed_result =
        installdb.evaluate_pkg_request_build_pars(pkg_request.name, pkg_request.b);
    CHECK(installed_result.size() == pkg_request.b.configs.size());

    std::map<config_name_t, vector<string>> build_reasons;

    for (auto& kv : installed_result) {
        const auto& config = kv.first;
        const auto& current_install_details = kv.second;
        const auto& current_install_desc = current_install_details.installed_config_desc;
        switch (kv.second.status) {
            case pkg_request_not_installed:
                build_reasons[config] = {"initial build"};
                break;
            case pkg_request_not_compatible: {
                auto& br = build_reasons[config];
                br = {stringf("build options changed")};
                br.emplace_back(stringf(
                    "CMAKE_ARGS of the currently installed build: %s",
                    join(normalize_cmake_args(current_install_desc.b.cmake_args), " ").c_str()));
                br.emplace_back(stringf("Requested CMAKE_ARGS: %s",
                                        join(pkg_request.b.cmake_args, " ").c_str()));
                br.emplace_back(stringf("Incompatible CMAKE_ARGS: %s",
                                        current_install_details.incompatible_cmake_args.c_str()));
            } break;
            case pkg_request_satisfied: {
                // test for new commits or uncommited changes
                if (cloned) {
                    if (cloned_sha == k_sha_uncommitted)
                        build_reasons[config] = {"workspace contains uncommited changes"};
                    else if (cloned_sha != current_install_desc.c.git_tag)
                        build_reasons[config] = {
                            "workspace is at a new commit",
                            stringf("Currently installed from commit: %s",
                                    current_install_desc.c.git_tag.c_str()),
                            string("Current commit in workspace: %s", cloned_sha.c_str())};
                }
                if (build_reasons.count(config) == 0) {
                    // examine each dependency
                    // collect all dependencies

                    auto deps = keys_of_map(current_install_desc.deps_shas);
                    append(deps, keys_of_map(pkg_request.deps_shas));
                    std::sort(BEGINEND(deps));
                    sx::unique_trunc(deps);

                    vector<string> new_deps;
                    vector<string> removed_deps;
                    vector<string> changed_deps;

                    for (auto& d : deps) {
                        auto it_inst = current_install_desc.deps_shas.find(d);
                        auto it_req = pkg_request.deps_shas.find(d);
                        if (it_inst == current_install_desc.deps_shas.end()) {
                            if (it_req == pkg_request.deps_shas.end()) {
                                CHECK(false);  // it must be in either
                            } else {
                                // dependency in current request but not in installed config
                                new_deps.emplace_back(d);
                            }
                        } else {
                            if (it_req == pkg_request.deps_shas.end()) {
                                // dependency in installed config but not in current request
                                removed_deps.emplace_back(d);
                            } else {
                                // dependency present both in installed config and current request
                                // is it in the identical config?
                                if (it_inst->second.count(config) > 0) {
                                    // compare only this SHA

                                } else {
                                }
                            }
                        }
                    }
                }
            } break;
            default:
                CHECK(false);
        }
        if (must_build)
            break;
    }

    // todo deps_shas doesn't seem to be filled

    string clone_dir = cfg.pkg_clone_dir(pkg_request.name);

    if (must_build && !cloned)
        clone_this();

    string unresolved_git_tag = pkg_request.c.git_tag;

    // todo: rather we need to remember if we're keeping the current install, because we it's
    // installed at the requested commit, or we'll build it
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

    idpo_recursion_result_t rr;

    // todo: we need to return from install_deps_phase_one if we're going to build a dependency
    // which should trigger build here, too

    // note: for now, since a dependency with uncommitteed changes will always be built, all the
    // packages that depend on it will be rebuilt

    // process_deps
    {
        if (cloned) {
            wsp.requester_stack.emplace_back(pkg_request.name);

            rr = install_deps_phase_one(binary_dir, pkg_source_dir,
                                        keys_of_map(pkg_request.deps_shas), global_cmake_args,
                                        configs, wsp, cmakex_cache);

            CHECK(wsp.requester_stack.back() == pkg_request.name);
            wsp.requester_stack.pop_back();
        } else {
            // enumerate dependencies from description of installed package

            // todo rather we need to call run_deps_add_pkgs
            for (auto& kv : installed_result) {
                CHECK(kv.second.status == pkg_request_satisfied);
                auto deps = keys_of_map(kv.second.installed_config_desc.deps_shas);
                append(rr.pkgs_encountered, deps);
            }
        }
    }

    auto& pm = wsp.pkg_map[pkg_request.name];
    auto& pd = pm.planned_desc;
    if (already_processed) {
        // todo: it is an error to add a dependency with different configurations then before
        // todo:: or it's an error to add it with different dependencies

        // extend configs if needed
        pd.b.configs.insert(pd.b.configs.end(), BEGINEND(pkg_request.b.configs));
        std::sort(BEGINEND(pd.b.configs));
        sx::unique_trunc(pd.b.configs);
        // extend depends if needed
        for (auto& d : rr.pkgs_encountered) {
            if (pd.deps_shas.count(d) == 0)
                pd.deps_shas[d];  // insert empty string
        }
    } else {
        wsp.build_order.push_back(pkg_request.name);
        pm.unresolved_git_tag = unresolved_git_tag;
        pd = pkg_request;
        for (auto& d : rr.pkgs_encountered)
            pd.deps_shas[d];  // insert empty string
    }
    rr.add_pkg(pkg_request.name);
    // todo update rr.building flag
    return rr;
}
}
