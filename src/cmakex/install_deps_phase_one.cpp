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

idpo_recursion_result_t install_deps_phase_one_deps_script(
    string_par binary_dir,
    string_par deps_script_filename,
    const vector<string>& command_line_cmake_args,
    const vector<config_name_t>& configs,
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache);

// todo what if the existing r1 is name-only, r2 is normal
// is it possible or what to do then?
void verify_if_requests_are_compatible(const pkg_request_t& r1, const pkg_request_t& r2)
{
    CHECK(r1.name == r2.name);
    CHECK(!r1.name_only());
    CHECK(!r2.name_only());
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
    auto& ec1 = b1.configs();
    CHECK(!ec1.empty());
    const char* fcl = " (from command line)";
    const char* fcl1 = b1.using_default_configs() ? fcl : "";
    auto& ec2 = b2.configs();
    CHECK(!ec2.empty());
    const char* fcl2 = b2.using_default_configs() ? fcl : "";

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
    auto& d1 = r1.depends;
    auto& d2 = r2.depends;

    if (d1 != d2)
        throwf(
            "Different dependecies requested for the same package. Previously, for package "
            "'%s' these dependencies were requested: [%s], now these: [%s]",
            pkg_name.c_str(), join(d1, ", ").c_str(), join(d2, ", ").c_str());
}

// if it's not there, insert
// if it's there:
// - if it's compatible (identical), do nothing
// - if it's not compatible:
//   - if the existing is name-only: check if it's not yet processed and overwrite
//   - throw otherwise
void insert_new_request_into_wsp(pkg_request_t&& req, deps_recursion_wsp_t& wsp)
{
    auto it = wsp.pkg_map.find(req.name);
    const bool to_be_processed = wsp.pkgs_to_process.count(req.name) > 0;
    if (it == wsp.pkg_map.end()) {
        // first time we encounter this package
        CHECK(!to_be_processed);
        wsp.pkgs_to_process.insert(req.name);
        wsp.pkg_map.emplace(std::piecewise_construct, std::forward_as_tuple(req.name),
                            std::forward_as_tuple(move(req)));
    } else {
        if (it->second.request.name_only()) {
            CHECK(to_be_processed);
            if (!req.name_only())
                it->second.request = move(req);
        } else {
            if (!req.name_only())
                verify_if_requests_are_compatible(it->second.request, req);
        }
    }
}

idpo_recursion_result_t process_pkgs_to_process(string_par binary_dir,
                                                const vector<string>& command_line_cmake_args,
                                                const vector<config_name_t>& configs,
                                                deps_recursion_wsp_t& wsp,
                                                const cmakex_cache_t& cmakex_cache)
{
    idpo_recursion_result_t rr;

    while (!wsp.pkgs_to_process.empty()) {
        string pkg_name;
        {
            auto it_begin = wsp.pkgs_to_process.begin();
            pkg_name = *it_begin;
            wsp.pkgs_to_process.erase(it_begin);
        }
        auto rr_below = run_deps_add_pkg(pkg_name, binary_dir, command_line_cmake_args, configs,
                                         wsp, cmakex_cache);
        rr.add(rr_below);
    }

    return rr;
}

idpo_recursion_result_t install_deps_phase_one_request_deps(
    string_par binary_dir,
    const vector<string>& request_deps,
    const vector<string>& command_line_cmake_args,
    const vector<config_name_t>& configs,
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache)
{
    // for each pkg:
    for (auto& d : request_deps)
        insert_new_request_into_wsp(pkg_request_t(d, configs, true), wsp);

    return process_pkgs_to_process(binary_dir, command_line_cmake_args, configs, wsp, cmakex_cache);
}

idpo_recursion_result_t install_deps_phase_one(string_par binary_dir,
                                               string_par source_dir,
                                               const vector<string>& request_deps,
                                               const vector<string>& command_line_cmake_args,
                                               const vector<config_name_t>& configs,
                                               deps_recursion_wsp_t& wsp,
                                               const cmakex_cache_t& cmakex_cache,
                                               string_par custom_deps_script_file)
{
    CHECK(!binary_dir.empty());
    CHECK(!configs.empty());
    if (!source_dir.empty() || !custom_deps_script_file.empty()) {
        string deps_script_file = fs::lexically_normal(
            custom_deps_script_file.empty()
                ? fs::absolute(source_dir.str()).string() + "/" + k_deps_script_filename
                : custom_deps_script_file.str());
        if (fs::is_regular_file(deps_script_file)) {
            if (!request_deps.empty())
                log_warn("Using dependency script \"%s\" instead of specified dependencies.",
                         deps_script_file.c_str());
            return install_deps_phase_one_deps_script(
                binary_dir, deps_script_file, command_line_cmake_args, configs, wsp, cmakex_cache);
        }
    }
    return install_deps_phase_one_request_deps(binary_dir, request_deps, command_line_cmake_args,
                                               configs, wsp, cmakex_cache);
}

idpo_recursion_result_t install_deps_phase_one_deps_script(string_par binary_dir,
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

    log_info("Processing dependency script: \"%s\"", deps_script_file.c_str());
    HelperCmakeProject hcp(binary_dir);
    auto addpkgs_lines = hcp.run_deps_script(deps_script_file);

    // for each pkg:
    for (auto& addpkg_line : addpkgs_lines) {
        auto args = split(addpkg_line, '\t');
        insert_new_request_into_wsp(pkg_request_t(pkg_request_from_args(args, configs)), wsp);
    }

    return process_pkgs_to_process(binary_dir, global_cmake_args, configs, wsp, cmakex_cache);
}

idpo_recursion_result_t run_deps_add_pkg(string_par pkg_name,
                                         string_par binary_dir,
                                         const vector<string>& command_line_cmake_args,
                                         const vector<config_name_t>& configs,
                                         deps_recursion_wsp_t& wsp,
                                         const cmakex_cache_t& cmakex_cache)
{
    const cmakex_config_t cfg(binary_dir);

    CHECK(wsp.pkg_map.count(pkg_name.str()) > 0);
    auto& pkg = wsp.pkg_map.at(pkg_name.str());

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

    // todo: get data from optional package registry here

    InstallDB installdb(binary_dir);

    // verify if this package is installed on one of the available prefix paths (from
    // CMAKE_PREFIX_PATH cmake and environment variables)
    // - it's an error if it's installed on multiple paths
    // - if it's installed on a prefix path we accept that installed package as it is
    auto prefix_paths = stable_unique(
        concat(cmakex_cache.cmakex_prefix_path_vector, cmakex_cache.env_cmakex_prefix_path_vector));
    if (!prefix_paths.empty()) {
        vector<config_name_t> configs_on_prefix_path;
        tie(pkg.found_on_prefix_path, configs_on_prefix_path) =
            installdb.quick_check_on_prefix_paths(pkg_name, prefix_paths);
        vector<config_name_t> requested_configs = pkg.request.b.configs();
        CHECK(!requested_configs
                   .empty());  // even for name-only requests must contain the default configs

        // overwrite requested configs with the installed ones
        auto cr = requested_configs;
        std::sort(BEGINEND(cr));
        sx::unique_trunc(cr);
        std::sort(BEGINEND(configs_on_prefix_path));

        // in case the requested configs and the installed configs are different, we're accepting
        // the installed configs
        string cmsg;
        if (cr != configs_on_prefix_path) {
            cmsg = stringf(
                ", accepting installed configuration%s (%s) instead of the requested one%s (%s)%s",
                configs_on_prefix_path.size() > 1 ? "s" : "",
                join(get_prefer_NoConfig(configs_on_prefix_path), ", ").c_str(),
                cr.size() > 1 ? "s" : "", join(get_prefer_NoConfig(cr), ", ").c_str(),
                pkg.request.b.using_default_configs() ? " (from the command line)" : "");
            pkg.request.b.update_configs(configs, false);
        } else
            cmsg =
                stringf(" (%s)", join(get_prefer_NoConfig(configs_on_prefix_path), ", ").c_str());

        if (!pkg.found_on_prefix_path.empty())
            log_info("Using %s from %s%s.", pkg_for_log(pkg_name).c_str(),
                     path_for_log(pkg.found_on_prefix_path).c_str(), cmsg.c_str());
        // in case the request is only a name, we're accepting the installed desc as request
    }

    struct per_config_data_t
    {
        bool initial_build;
    };

    std::map<config_name_t, per_config_data_t> pcd;

    {
        bool first_iteration = true;
        for (auto& c : pkg.request.b.configs()) {
            auto& pcd_c = pcd[c];
            auto& pkg_c = pkg.pcd[c];
            if (cmakex_cache.per_config_bin_dirs || first_iteration) {
                auto pkg_bin_dir_of_config =
                    cfg.pkg_binary_dir_of_config(pkg_name, c, cmakex_cache.per_config_bin_dirs);
                pcd_c.initial_build =
                    !fs::is_regular_file(pkg_bin_dir_of_config + "/CMakeCache.txt");

                auto& cmake_args_to_apply = pkg_c.cmake_args_to_apply;

                if (pcd_c.initial_build) {
                    auto pkg_cmake_args = normalize_cmake_args(pkg.request.b.cmake_args);
                    auto* cpp =
                        find_specific_cmake_arg_or_null("CMAKE_INSTALL_PREFIX", pkg_cmake_args);
                    CHECK(!cpp,
                          "Internal error: package CMAKE_ARGS's should not change "
                          "CMAKE_INSTALL_PREFIX: '%s'",
                          cpp->c_str());
                    cmake_args_to_apply = pkg_cmake_args;
                    cmake_args_to_apply.emplace_back("-DCMAKE_INSTALL_PREFIX=" +
                                                     cfg.deps_install_dir());
                }

                {
                    auto* cpp = find_specific_cmake_arg_or_null("CMAKE_INSTALL_PREFIX",
                                                                command_line_cmake_args);
                    CHECK(!cpp,
                          "Internal error: command-line CMAKE_ARGS's forwarded to a dependency "
                          "should not change CMAKE_INSTALL_PREFIX: '%s'",
                          cpp->c_str());
                }

                cmake_args_to_apply =
                    normalize_cmake_args(concat(cmake_args_to_apply, command_line_cmake_args));

                cmake_args_to_apply = make_sure_cmake_path_var_contains_path(
                    pkg_bin_dir_of_config, "CMAKE_MODULE_PATH", cfg.find_module_hijack_dir(),
                    cmake_args_to_apply);

                // get tentative per-config final_cmake_args from cmake cache tracker by applying
                // these cmake_args onto the current tracked values
                auto cct = load_cmake_cache_tracker(pkg_bin_dir_of_config);
                cct.add_pending(cmake_args_to_apply);
                cct.confirm_pending();
                pkg_c.tentative_final_cmake_args = cct.cached_cmake_args;
            } else {
                auto first_c = *pkg.request.b.configs().begin();
                pcd_c.initial_build = pcd[first_c].initial_build;
                pkg_c = pkg.pcd[first_c];
            }
        }
    }

    // if it's already installed we still need to process this:
    // - to enumerate all dependencies
    // - to check if only compatible installations are requested

    clone_helper_t clone_helper(binary_dir, pkg_name);
    auto& cloned = clone_helper.cloned;
    auto& cloned_sha = clone_helper.cloned_sha;

    auto clone_this_at_sha = [&pkg, &clone_helper](string sha) {
        auto prc = pkg.request.c;
        if (!sha.empty())
            prc.git_tag = sha;
        clone_helper.clone(prc, pkg.request.git_shallow);
        pkg.just_cloned = true;
    };

    auto clone_this = [&clone_this_at_sha]() { clone_this_at_sha({}); };

    string pkg_source_dir = cfg.pkg_clone_dir(pkg_name);
    if (!pkg.request.b.source_dir.empty())
        pkg_source_dir += "/" + pkg.request.b.source_dir;

    if (pkg.request.name_only() && pkg.found_on_prefix_path.empty())
        throwf("No definition found for package %s", pkg_for_log(pkg_name).c_str());

    auto per_config_final_cmake_args = [&pkg]() {
        std::map<config_name_t, vector<string>> r;
        for (auto& c : pkg.request.b.configs())
            r[c] = pkg.pcd.at(c).tentative_final_cmake_args;
        return r;
    };

    // determine installed status
    auto installed_result = installdb.evaluate_pkg_request_build_pars(
        pkg_name, pkg.request.b.source_dir, per_config_final_cmake_args(),
        pkg.found_on_prefix_path);
    CHECK(installed_result.size() == pkg.request.b.configs().size());

    // if it's not found on a prefix path
    if (pkg.found_on_prefix_path.empty()) {
        // if any of the requested configs is not satisfied we know we need the clone right now
        // this is partly a shortcut, partly later (when traversing the dependencies) we
        // exploit the fact that this package is either installed (say, from server) or cloned
        bool one_config_is_not_satisfied = false;
        for (auto& c : pkg.request.b.configs()) {
            if (installed_result.at(c).status != pkg_request_satisfied) {
                one_config_is_not_satisfied = true;
                break;
            }
        }
        if (one_config_is_not_satisfied && !cloned)
            clone_this();
    } else {
        // at this point, if we found it on prefix path, we've already overwritten the requested
        // config with the installed one. Verify this.
        auto rcs = pkg.request.b.configs();
        std::sort(BEGINEND(rcs));
        CHECK(rcs == keys_of_map(installed_result));
        CHECK(rcs == keys_of_map(pkg.pcd));

        // if it's found on a prefix_path it must be good as it is, we can't change those
        // also, it must not be cloned
        if (cloned) {
            throwf(
                "%s found on the prefix path %s but and it's also checked out in %s. Remove either "
                "from the prefix path or from the directory.",
                pkg_for_log(pkg_name).c_str(), path_for_log(pkg.found_on_prefix_path).c_str(),
                path_for_log(cfg.pkg_clone_dir(pkg_name)).c_str());
        }
        // if it's name-only then we accept it always
        if (pkg.request.name_only()) {
            // overwrite with actual installed descs
            // the installed descs must be uniform, that is, same settings for each installed config

            auto& first_desc = installed_result.begin()->second.installed_config_desc;
            const char* different_option = nullptr;
            for (auto& kv : installed_result) {
                auto& desc = kv.second.installed_config_desc;
                CHECK(desc.pkg_name == pkg_name &&
                      desc.config == kv.first);  // the installdb should have verified this

                pkg.request.c.git_url = desc.git_url;
                if (desc.git_url != first_desc.git_url) {
                    different_option = "GIT_URL";
                    break;
                }
                pkg.request.c.git_tag = desc.git_sha;
                if (desc.git_sha != first_desc.git_sha) {
                    different_option = "GIT_SHA";
                    break;
                }

                pkg.request.b.source_dir = desc.source_dir;
                if (desc.source_dir != first_desc.source_dir) {
                    different_option = "SOURCE_DIR";
                    break;
                }

                auto& c = kv.first;
                pkg.pcd[c].tentative_final_cmake_args = desc.final_cmake_args;

                auto ds = keys_of_map(desc.deps_shas);
                pkg.request.depends.insert(BEGINEND(ds));

                if (ds != keys_of_map(first_desc.deps_shas)) {
                    different_option = "DEPENDS";
                    break;
                }
            }

            if (different_option) {
                throwf(
                    "Package %s found on the prefix path %s but its configurations (%s) have "
                    "been built with different build options. Offending option: %s.",
                    pkg_for_log(pkg_name).c_str(), path_for_log(pkg.found_on_prefix_path).c_str(),
                    join(get_prefer_NoConfig(keys_of_map(installed_result)), ", ").c_str(),
                    different_option);
            }

            // update installed_result
            installed_result = installdb.evaluate_pkg_request_build_pars(
                pkg_name, pkg.request.b.source_dir, per_config_final_cmake_args(),
                pkg.found_on_prefix_path);

            for (auto& kv : installed_result)
                CHECK(kv.second.status == pkg_request_satisfied);

        } else {
            for (auto& c : pkg.request.b.configs()) {
                auto& itc = installed_result.at(c);
                if (itc.status == pkg_request_not_compatible) {
                    throwf(
                        "Package %s found on the prefix path %s but the build options are not "
                        "compatible with the current ones. Either remove from the prefix path or "
                        "check build options. The offending CMAKE_ARGS: [%s]",
                        pkg_for_log(pkg_name).c_str(),
                        path_for_log(pkg.found_on_prefix_path).c_str(),
                        itc.incompatible_cmake_args.c_str());
                } else {
                    // it can't be not installed
                    CHECK(itc.status == pkg_request_satisfied);
                }
            }
        }
    }

    std::map<config_name_t, vector<string>> build_reasons;

    idpo_recursion_result_t rr;

    // if first attempt finds reason to build and this package is not cloned, a second attempt
    // will follow after a clone
    for (int attempts = 1;; ++attempts) {
        CHECK(attempts <= 2);  // before second iteration there will be a cloning so the second
                               // iteration must finish
        build_reasons.clear();
        rr.clear();
        bool for_clone_use_installed_sha = false;
        bool restore_wsp_before_second_attempt = false;

        using wsp_t = std::remove_reference<decltype(wsp)>::type;
        wsp_t saved_wsp;

        // process_deps
        if (cloned) {
            wsp.requester_stack.emplace_back(pkg_name);

            rr = install_deps_phase_one(binary_dir, pkg_source_dir, to_vector(pkg.request.depends),
                                        command_line_cmake_args, configs, wsp, cmakex_cache, "");

            CHECK(wsp.requester_stack.back() == pkg_name);
            wsp.requester_stack.pop_back();
        } else {
            if (!pkg.found_on_prefix_path.empty()) {
                rr = install_deps_phase_one_request_deps(
                    binary_dir, keys_of_set(pkg.request.depends), command_line_cmake_args, configs,
                    wsp, cmakex_cache);
            } else {
                // enumerate dependencies from description of installed package
                LOG_FATAL("This branch is not implemented");

                // todo implement this branch:
                // this package is installed but not cloned. So this has been remote built and
                // installed as headers+binary
                // the description that came with the binary should contain the detailed
                // descriptions (requests) of its all dependencies. It's like a deps.cmake file
                // 'materialized' that is, processed into a similar file we process the deps.cmake
                // and also git_tags resolved to concrete SHAs.
                // The dependencies will be either built locally or downloaded based on that
                // description

                saved_wsp = wsp;

                restore_wsp_before_second_attempt = true;
                for_clone_use_installed_sha = true;

#if 0
            rr = install_deps_phase_one_remote_build(pkg_name, command_line_cmake_args, configs, wsp,
                                                     cmakex_cache);
#endif
            }
        }

        if (rr.building_some_pkg) {
            for (auto& c : pkg.request.b.configs()) {
                auto status = installed_result.at(c).status;
                if (status != pkg_request_not_installed && status != pkg_request_not_compatible)
                    build_reasons[c].assign(1, "a dependency has been rebuilt");
            }
        }

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
                    br.emplace_back(stringf(
                        "CMAKE_ARGS of the currently installed build: %s",
                        join(normalize_cmake_args(current_install_desc.final_cmake_args), " ")
                            .c_str()));
                    br.emplace_back(
                        stringf("Requested CMAKE_ARGS: %s",
                                join(pkg.pcd.at(config).tentative_final_cmake_args, " ").c_str()));
                    br.emplace_back(
                        stringf("Incompatible CMAKE_ARGS: %s",
                                current_install_details.incompatible_cmake_args.c_str()));
                } break;
                case pkg_request_satisfied: {
                    // test for new commits or uncommited changes
                    if (cloned) {
                        if (cloned_sha == k_sha_uncommitted) {
                            build_reasons[config] = {"workspace contains uncommited changes"};
                            break;
                        } else if (cloned_sha != current_install_desc.git_sha) {
                            build_reasons[config] = {
                                "workspace is at a new commit",
                                stringf("Currently installed from commit: %s",
                                        current_install_desc.git_sha.c_str()),
                                string("Current commit in workspace: %s", cloned_sha.c_str())};
                            break;
                        }
                    }
                    // examine each dependency
                    // collect all dependencies

                    auto deps =
                        concat(keys_of_map(current_install_desc.deps_shas), pkg.request.depends);
                    std::sort(BEGINEND(deps));
                    sx::unique_trunc(deps);

                    for (auto& d : deps) {
                        // we can stop at first reason to build
                        if (build_reasons.count(config) > 0)
                            break;
                        if (pkg.request.depends.count(d) == 0) {
                            // this dependency is not requested now (but was needed when this
                            // package was installed)
                            build_reasons[config] = {
                                stringf("dependency '%s' is not needed any more", d.c_str())};
                            break;
                        }

                        // this dependency is requested now

                        // the configs of this dependency that was needed when this package was
                        // build
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
                            // it's not installed now but it's requested. This should have
                            // triggered
                            // building that dependency and then building this package in turn.
                            // Since we're here it did not happened so this is an internal
                            // error.
                            LOG_FATAL(
                                "Package '%s', dependency '%s' is not installed but requested "
                                "but "
                                "still it didn't trigger building this package.",
                                pkg_name.c_str(), d.c_str());
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
            }  // switch on the evaluation result between request config and installed config
        }      // for all requested configs

        if (cloned && wsp.force_build) {
            for (auto& c : pkg.request.b.configs()) {
                if (build_reasons.count(c) == 0)
                    build_reasons[c].assign(1, "'--force-build' specified.");
            }
        }

        if (build_reasons.empty() || cloned)
            break;
        if (restore_wsp_before_second_attempt)
            wsp = move(saved_wsp);
        if (for_clone_use_installed_sha) {
            // if it's installed at some commit, clone that commit
            string installed_sha;
            for (auto& kv : installed_result) {
                if (kv.second.status == pkg_request_not_installed)
                    continue;
                string cs = kv.second.installed_config_desc.git_sha;
                if (!sha_like(cs))
                    throwf(
                        "About to clone the already installed %s (need to be rebuilt). Can't "
                        "decide which commit to clone since it has been installed from an "
                        "invalid commit. Probably the workspace contaniner local changes. The "
                        "offending SHA: %s",
                        pkg_for_log(pkg_name).c_str(), cs.c_str());
                if (installed_sha.empty())
                    installed_sha = cs;
                else if (installed_sha != cs) {
                    throwf(
                        "About to clone the already installed %s (needs to be rebuilt). Can't "
                        "decide which commit to clone since different configurations has been "
                        "installed from different commits. Two offending commit SHAs: %s and "
                        "%s",
                        pkg_for_log(pkg_name).c_str(), cloned_sha.c_str(), cs.c_str());
                }
            }
            CHECK(!installed_sha.empty());  // it can't be empty since if it's not installed
            // then we should not get here
            clone_this_at_sha(installed_sha);
        } else
            clone_this();
    }  // for attempts

    string clone_dir = cfg.pkg_clone_dir(pkg_name);

    // for now the first request will determine which commit to build
    // and second, different request will be an error
    // If that's not good, relax and implement some heuristics

    if (!build_reasons.empty()) {
        CHECK(cloned);
        wsp.build_order.push_back(pkg_name.str());
        pkg.resolved_git_tag = cloned_sha;
        for (auto& kv : build_reasons)
            pkg.pcd.at(kv.first).build_reasons = kv.second;
        rr.building_some_pkg = true;
    }
    rr.add_pkg(pkg_name);
    return rr;
}
}
