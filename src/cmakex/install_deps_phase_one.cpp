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
    append_inplace(pkgs_encountered, x.pkgs_encountered);
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
                                                            const vector<string>& request_deps,
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
                                               const vector<string>& request_deps,
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
        if (fs::is_regular_file(deps_script_file)) {
            if (!request_deps.empty())
                log_warn("Using dependency script \"%s\" instead of specified dependencies.",
                         deps_script_file.c_str());
            return install_deps_phase_one_deps_script(
                binary_dir, deps_script_file, global_cmake_args, configs, wsp, cmakex_cache);
        }
    }
    return install_deps_phase_one_request_deps(binary_dir, request_deps, global_cmake_args, configs,
                                               wsp, cmakex_cache);
}

// todo: is it still a problem when we're build Debug from all packages, then Release (separate
// command) then Debug again and it will try to rebuild almost all the packages?

void verify_if_requests_are_compatible(const pkg_request_t& r1,
                                       const pkg_request_t& r2,
                                       const vector<config_name_t>& configs)
{
    CHECK(r1.name == r2.name);
    auto& pkg_name = r1.name;

    // check for possible conflicts with existing requests
    auto& b1 = r1.b;
    auto& b2 = r2.b;
    auto& c1 = r1.c;
    auto& c2 = r2.c;

    // compare SOURCE_DIR
    auto& s1 = b1.source_dir;
    auto& s2 = b2.source_dir;
    if (s1 != s2) {
        throwf(
            "Different SOURCE_DIR args for the same package. The package '%s' is being "
            "added "
            "for the second time. The first time the SOURCE_DIR was \"%s\" and not it's "
            "\"%s\".",
            pkg_name.c_str(), s1.c_str(), s2.c_str());
    }

    // compare CMAKE_ARGS
    auto v = incompatible_cmake_args(b1.cmake_args, b2.cmake_args);
    if (!v.empty()) {
        throwf(
            "Different CMAKE_ARGS args for the same package. The package '%s' is being "
            "added "
            "for the second time but the following CMAKE_ARGS are incompatible with the "
            "first "
            "request: %s",
            pkg_name.c_str(), join(v, ", ").c_str());
    }

    // compare configs
    auto ec1 = b1.configs;
    const char* fcl = " (from command line)";
    const char* fcl1 = "";
    if (ec1.empty()) {
        ec1 = configs;
        fcl1 = fcl;
    }
    const char* fcl2 = "";
    auto ec2 = b2.configs;
    if (ec2.empty()) {
        ec2 = configs;
        fcl2 = fcl;
    }

    std::sort(BEGINEND(ec1));
    std::sort(BEGINEND(ec2));
    sx::unique_trunc(ec1);
    sx::unique_trunc(ec2);

    if (ec1 != ec2) {
        auto v1 = join(get_prefer_NoConfig(ec1), ", ");
        auto v2 = join(get_prefer_NoConfig(ec2), ", ");
        throwf(
            "Different configurations requested for the same package. Previously, for "
            "package "
            "'%s' these configurations had been requested: [%s]%s and now these: [%s]%s",
            pkg_name.c_str(), v1.c_str(), fcl1, v2.c_str(), fcl2);
    }

    // compare clone pars
    if (c1.git_url != c2.git_url)
        throwf(
            "Different repository URLs requested for the same package. Previously, for "
            "package "
            "'%s' this URL was specified: %s, now this: %s",
            pkg_name.c_str(), c1.git_url.c_str(), c2.git_url.c_str());
    if (c1.git_tag != c2.git_tag)
        throwf(
            "Different commits requested for the same package. Previously, for package "
            "'%s' this GIT_TAG was specified: %s, now this: %s",
            pkg_name.c_str(), c1.git_tag.c_str(), c2.git_tag.c_str());

    // compare dependencies
    auto d1 = keys_of_map(r1.deps_shas);
    auto d2 = keys_of_map(r2.deps_shas);

    if (d1 != d2)
        throwf(
            "Different dependecies requested for the same package. Previously, for package "
            "'%s' these dependencies were requested: [%s], now these: [%s]",
            pkg_name.c_str(), join(d1, ", ").c_str(), join(d2, ", ").c_str());
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
        auto args = split(addpkg_line, '\t');
        auto pkg_request = pkg_request_from_args(args);
        auto it = wsp.pkg_map.find(pkg_request.name);
        if (it == wsp.pkg_map.end()) {
            // first time we encounter this package
            CHECK(wsp.pkgs_to_process.count(pkg_request.name) == 0);
            wsp.pkgs_to_process.insert(pkg_request.name);
            wsp.pkg_map[pkg_request.name].request = move(pkg_request);
        } else {
            verify_if_requests_are_compatible(it->second.request, pkg_request, configs);
        }
    }

    while (!wsp.pkgs_to_process.empty()) {
        string pkg_name;
        {
            auto it_begin = wsp.pkgs_to_process.begin();
            pkg_name = *it_begin;
            wsp.pkgs_to_process.erase(it_begin);
        }
        auto rr_below =
            run_deps_add_pkg(pkg_name, binary_dir, global_cmake_args, configs, wsp, cmakex_cache);
        rr.add(rr_below);
    }

    return rr;
}

idpo_recursion_result_t run_deps_add_pkg(string_par pkg_name,
                                         string_par binary_dir,
                                         const vector<string>& global_cmake_args,
                                         const vector<config_name_t>& configs,
                                         deps_recursion_wsp_t& wsp,
                                         const cmakex_cache_t& cmakex_cache)
{
    const cmakex_config_t cfg(binary_dir);

    CHECK(wsp.pkg_map.count(pkg_name.str()) > 0);
    auto& pkg = wsp.pkg_map.at(pkg_name.str());

    // add global cmake args
    pkg.final_cmake_args =
        normalize_cmake_args(concat(global_cmake_args, pkg.request.b.cmake_args));

    if (linear_search(wsp.requester_stack, pkg_name)) {
        // report circular dependency error
        string s;
        for (auto& x : wsp.requester_stack) {
            if (!s.empty())
                s += " -> ";
            s += x;
        }
        s += "- > ";
        s += pkg_name;
        throwf("Circular dependency: %s", s.c_str());
    }

    // if it's already installed we still need to process this:
    // - to enumerate all dependencies
    // - to check if only compatible installations are requested

    // todo: get data from optional package registry here

    // fill configs from global par if not specified
    if (pkg.request.b.configs.empty())
        pkg.request.b.configs = configs;

    pkg.request.b.configs = stable_unique(pkg.request.b.configs);

    // it's already processed we need to check if it's the same request as before
    const bool already_processed = wsp.pkg_map.count(pkg_name.str()) > 0;

    clone_helper_t clone_helper(binary_dir, pkg_name);
    auto& cloned = clone_helper.cloned;
    auto& cloned_sha = clone_helper.cloned_sha;

    auto clone_this = [&pkg, &wsp, &clone_helper, &pkg_name](string sha = "") {
        auto prc = pkg.request.c;
        if (!sha.empty())
            prc.git_tag = sha;
        clone_helper.clone(prc, pkg.request.git_shallow);
        wsp.pkg_map.at(pkg_name.str()).just_cloned = true;
    };

    string pkg_source_dir = cfg.pkg_clone_dir(pkg_name);
    if (!pkg.request.b.source_dir.empty())
        pkg_source_dir += "/" + pkg.request.b.source_dir;

    // determine installed status
    InstallDB installdb(binary_dir);
    auto installed_result = installdb.evaluate_pkg_request_build_pars(pkg_name, pkg.request.b);
    CHECK(installed_result.size() == pkg.request.b.configs.size());

    // if not installed we need the clone always
    if (installed_result.empty() && !cloned)
        clone_this();

    std::map<config_name_t, vector<string>> build_reasons;

    idpo_recursion_result_t rr;

    for (int attempts = 1;; ++attempts) {
        CHECK(attempts <= 2);  // before second iteration there will be a cloning so the second
                               // iteration must finish
        build_reasons.clear();

        // process_deps
        std::remove_reference<decltype(wsp)>::type saved_wsp;
        {
            if (cloned) {
                wsp.requester_stack.emplace_back(pkg_name);

                rr = install_deps_phase_one(binary_dir, pkg_source_dir,
                                            keys_of_map(pkg.request.deps_shas), global_cmake_args,
                                            configs, wsp, cmakex_cache);

                CHECK(wsp.requester_stack.back() == pkg_name);
                wsp.requester_stack.pop_back();
            } else {
                // enumerate dependencies from description of installed package

                // todo rather we need to call run_deps_add_pkgs
                // we need to store the resolved deps.cmake of the package
                for (auto& kv : installed_result) {
                    CHECK(kv.second.status == pkg_request_satisfied);
                    auto deps = concat(keys_of_map(kv.second.installed_config_desc.deps_shas),
                                       rr.pkgs_encountered);
                }
            }
        }
        current state because we may need to rerun;

        if (any - dep - is - building && !cloned) {
            restore saved state;
            build_reason =
                rerunning(will not be printed)  // will skip next loop over installed configs
        } else
            commit new state;

        for (auto& kv : installed_result) {
            const auto& config = kv.first;
            if (build_reasons.count(config) > 0)
                break;  // need only one reason
            const auto& current_install_details = kv.second;
            const auto& current_install_desc = current_install_details.installed_config_desc;
            switch (kv.second.status) {
                case pkg_request_not_installed:
                    build_reasons[config] = {"initial build"};
                    break;
                case pkg_request_not_compatible: {
                    auto& br = build_reasons[config];
                    br = {stringf("build options changed")};
                    br.emplace_back(
                        stringf("CMAKE_ARGS of the currently installed build: %s",
                                join(normalize_cmake_args(current_install_desc.b.cmake_args), " ")
                                    .c_str()));
                    br.emplace_back(stringf("Requested CMAKE_ARGS: %s",
                                            join(pkg.request.b.cmake_args, " ").c_str()));
                    br.emplace_back(
                        stringf("Incompatible CMAKE_ARGS: %s",
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
                    // examine each dependency
                    // collect all dependencies

                    auto deps = concat(keys_of_map(current_install_desc.deps_shas),
                                       keys_of_map(pkg.request.deps_shas));
                    std::sort(BEGINEND(deps));
                    sx::unique_trunc(deps);

                    for (auto& d : deps) {
                        // we can stop at first reason to build
                        if (build_reasons.count(config) > 0)
                            break;
                        if (pkg.request.deps_shas.count(d) == 0) {
                            // this dependency is not requested now (but was needed when this
                            // package was installed)
                            build_reasons[config] = {
                                stringf("dependency '%s' is not needed any more", d.c_str())};
                            break;
                        }

                        // this dependency is requested now

                        // the configs this dependency was needed when this package was build
                        // and installed
                        auto it_previnst_dep_configs = current_install_desc.deps_shas.find(d);
                        if (it_previnst_dep_configs == current_install_desc.deps_shas.end()) {
                            // this dep is requested but was not present at the previous
                            // installation
                            build_reasons[config] = {stringf("new dependency: '%s'", d.c_str())};
                            break;
                        }

                        // this dep is requested, was present at the previous installation
                        // check if it's installed now
                        auto dep_current_install = installdb.try_get_installed_pkg_all_configs(d);
                        if (dep_current_install.empty()) {
                            // it's not installed now but it's requested. This would trigger
                            // building that dependency and then this package in turn
                            // so let's just leave now and let the missing dependency trigger
                            // these
                            break;
                        }

                        // this dependency is installed now
                        bool this_dep_config_was_installed =
                            it_previnst_dep_configs->second.count(config) > 0;
                        bool this_dep_config_is_installed =
                            dep_current_install.config_descs.count(config) > 0;

                        if (this_dep_config_is_installed != this_dep_config_was_installed) {
                            build_reasons[config] = {
                                stringf("dependency '%s' / config '%s' has been changed since "
                                        "last install",
                                        d.c_str(), config.get_prefer_NoConfig().c_str())};
                            break;
                        }

                        if (this_dep_config_was_installed) {
                            // this config of the dependency was installed and is
                            // installed
                            // check if it has been changed
                            string dep_current_sha =
                                calc_sha(dep_current_install.config_descs.at(config));
                            string dep_previnst_sha = it_previnst_dep_configs->second.at(config);
                            if (dep_current_sha != dep_previnst_sha) {
                                build_reasons[config] = {
                                    stringf("dependency '%s' has been changed since "
                                            "last install",
                                            d.c_str())};
                                break;
                            }
                        } else {
                            // this dep config was not installed and is not installed
                            // now (but other configs of this dependency were and are)
                            // that means, all those configs must be unchanged to prevent
                            // build
                            auto& map1 = dep_current_install.config_descs;
                            auto& map2 = it_previnst_dep_configs->second;
                            vector<config_name_t> configs =
                                concat(keys_of_map(map1), keys_of_map(map2));
                            std::sort(BEGINEND(configs));
                            sx::unique_trunc(configs);
                            config_name_t* changed_config = nullptr;
                            for (auto& c : configs) {
                                auto it1 = map1.find(c);
                                auto it2 = map2.find(c);
                                if ((it1 == map1.end()) != (it2 == map2.end())) {
                                    changed_config = &c;
                                    break;
                                }
                                CHECK(it1 != map1.end() && it2 != map2.end());
                                if (calc_sha(it1->second) != it2->second) {
                                    changed_config = &c;
                                    break;
                                }
                            }
                            if (changed_config) {
                                build_reasons[config] = {
                                    stringf("dependency '%s' / config '%s' has been changed "
                                            "since last install",
                                            d.c_str(), config.get_prefer_NoConfig().c_str())};
                                break;
                            }
                        }
                    }
                } break;
                default:
                    CHECK(false);
            }
        }

        bool rerun = false;
        if (!build_reasons.empty() && !cloned) {
            // if it's installed at some commit, clone that commit
            string cloned_sha;
            for (auto& kv : installed_result) {
                if (kv.second.status != pkg_request_not_installed) {
                    string cs = kv.second.installed_config_desc.c.git_tag;
                    if (!sha_like(cs))
                        throwf(
                            "About to clone the already installed %s (need to be rebuilt). Can't "
                            "decide which commit to clone since it has been installed from an "
                            "invalid "
                            "commit. Probably the workspace contaniner local changes. The "
                            "offending "
                            "SHA: %s",
                            pkg_for_log(pkg_name).c_str(), cs.c_str());
                    if (cloned_sha.empty())
                        cloned_sha = cs;
                    else if (cloned_sha != cs) {
                        throwf(
                            "About to clone the already installed %s (need to be rebuilt). Can't "
                            "decide which commit to clone since different configurations has been "
                            "installed from different commits. Two offending commit SHAs: %s and "
                            "%s",
                            pkg_for_log(pkg_name).c_str(), cloned_sha.c_str(), cs.c_str());
                    }
                }
            }

            if (cloned_sha.empty())
                clone_this();
            else {
                clone_this(cloned_sha);  // clone exactly at the already installed commit
                // and re-run the entire function to make sure we're resolving the same dependencies
                // for now, the installation record specified the dependencies, after cloning the
                // deps.cmake
                // will be re-run
                rerun = true;
            }
        }
        if (!rerun)
            break;
    }

    // todo deps_shas doesn't seem to be filled
    // todo installed git sha must be current time if local changes

    string clone_dir = cfg.pkg_clone_dir(pkg_name);

    string unresolved_git_tag = pkg.request.c.git_tag;

// todo for now tthe first request will determine which commit to build
// and second, different request will be an error
// If that's not good, relax and implement some heuristics
#if 0
    if (already_processed) {
        // verify final, resolved commit SHAs
        auto& pm = wsp.pkg_map[pkg_name];
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
#endif

    // todo: we need to return from install_deps_phase_one if we're going to build a dependency
    // which should trigger build here, too

    // note: for now, since a dependency with uncommitteed changes will always be built, all the
    // packages that depend on it will be rebuilt

    auto& pm = wsp.pkg_map[pkg_name.str()];
    auto& pd = pm.request;

    if (!already_processed) {
        wsp.build_order.push_back(pkg_name.str());
        pm.unresolved_git_tag = unresolved_git_tag;
        pd = pkg.request;
        for (auto& d : rr.pkgs_encountered)
            pd.deps_shas[d];  // insert empty string
    }
    rr.add_pkg(pkg_name);
    // todo update rr.building flag
    return rr;
}
}
