#include "install_deps_phase_two.h"

#include <adasworks/sx/algorithm.h>
#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

#include "build.h"
#include "clone.h"
#include "cmakex_utils.h"
#include "git.h"
#include "misc_utils.h"
#include "print.h"

namespace cmakex {

vector<string> replace_empty_config(vector<string> x)
{
    for (auto& c : x) {
        if (c.empty())
            c = "NoConfig";
    }
    return x;
}

void install_deps_phase_two(string_par binary_dir,
                            deps_recursion_wsp_t& wsp,
                            bool force_config_step)
{
// todo
#if 0
    log_info("");
    InstallDB installdb(binary_dir);
    for (auto& p : wsp.build_order) {
        vector<string> build_reasons;
        auto& wp = wsp.pkg_map[p];
        // request eval compares the request of this package and what is currently installed
        // and answers this for each requested config
        auto request_eval = installdb.evaluate_pkg_request_build_pars(p, wp.planned_desc.b);

        // record each dependency's current state and also remember changed deps along the way
        vector<string> deps_changed;
        for (auto& kv : wp.planned_desc.deps_shas) {
            auto& d = kv.first;
            // for each requested config of this package
            // check how this dependency had looked like when that config was installed
            // and how it looks like now
            auto dep_installed_configs = installdb.try_get_installed_pkg_all_configs(d);
            CHECK(dep_installed_configs.empty(),
                "Internal error: a dependency %s is not installed at the time the "
                "%s is about to build",
                pkg_for_log(d).c_str(), pkg_for_log(p).c_str());
            string dep_current_hash = dep_installed_configs.sha();
            CHECK(!dep_current_hash.empty());
            // for each requested config
            for (auto& kv : request_eval) {
                // if it's not satisfied we're going to build it anyway, no need to check dependency
                if (kv.second.status != pkg_request_satisfied)
                    continue;
                auto& deps_shas = kv.second.installed_config_desc.deps_shas;
                if (deps_shas.count(d) == 0 || deps_shas.at(d) != dep_current_hash)
                    deps_changed.emplace_back(d);
            }
            kv.second = dep_current_hash;
        }

        std::sort(BEGINEND(deps_changed));
        sx::unique_trunc(deps_changed);

        clone_helper_t clone_helper(binary_dir, p);

        // check if we still need to build it because of new commits or uncommited changes
        // - from the previous check re.status was pkg_request_satisfied
        // One possible reason we should still rebuild it is that the current workspace
        // has changed compared to what is currently installed.
        // We simply need to build if the current workspace and installed sha are different.
        if (clone_helper.cloned) {
            if (clone_helper.cloned_sha == k_sha_uncommitted) {
                build_reasons = { "workspace contains uncommited changes" };
            }
            else {
                vector<string> missing_configs;
                vector<string> incompatible_configs;
                for (auto& kv : request_eval) {
                    switch (kv.second.status) {
                    case pkg_request_satisfied:
                        //nothing to do here
                        break;
                    case pkg_request_not_installed:
                        missing_configs.emplace_back(kv.first);
                        break;
                    case pkg_request_not_compatible:
                        incompatible_configs.emplace_back(kv.first);
                        break;
                    default:
                        CHECK(false);
                    }
                }
                string b1, b2;
                if (!missing_configs.empty())
                    b1 = stringf("not installed (%s)", join(missing_configs, ", ").c_str());
                if (!incompatible_configs.empty())
                    b2 = stringf("different build options (%s)", join(incompatible_configs, ", ").c_str());
                if (b1.empty())
                    build_reasons = b2;
                else if (b2.empty())
                    build_reasons = b1;
                else if (!b1.empty() && !b2.empty())
                    build_reasons = b1 + ", " + b2;
            }
            else if (clone_helper.cloned_sha != request_eval.pkg_desc.c.git_tag)
            {
                build_reasons = { "workspace contains new commit" };
            }
            else if (request_eval.pkg_desc.c.git_tag.empty())
            {
                build_reasons = { "initial build" };
            }
        }

        // check if we need to build because of changed dependencies
        if (build_reasons.empty()) {
            // check if we still need to build it because of changed dependencies
            if (!deps_changed.empty())
                build_reasons = { stringf("dependencies changed since last build: %s",
                    join(deps_changed, ", ").c_str()) };
        }

        // check if we need to build because of some other reason, decided in phase one
        if (build_reasons.empty()) {
            switch (request_eval.status) {
            case pkg_request_satisfied:
                break;
            case pkg_request_missing_configs:
                build_reasons = { stringf(
                    "missing configurations (%s)",
                    join(replace_empty_config(request_eval.missing_configs), ", ").c_str()) };
                incremental_install = true;
                break;
            case pkg_request_not_installed:
                build_reasons = { "initial build" };
                break;
            case pkg_request_not_compatible:
                build_reasons = { stringf("build options changed") };
                build_reasons.emplace_back(stringf(
                    "CMAKE_ARGS of the currently installed build: %s",
                    join(normalize_cmake_args(wp.planned_desc.b.cmake_args), " ").c_str()));
                build_reasons.emplace_back(
                    stringf("Requested CMAKE_ARGS: %s",
                        join(request_eval.pkg_desc.b.cmake_args, " ").c_str()));
                build_reasons.emplace_back(
                    stringf("Incompatible CMAKE_ARGS: %s",
                        request_eval.incompatible_cmake_args.c_str()));
                break;
            default:
                CHECK(false);
            }
        }

        if (build_reasons.empty())
            continue;

        log_info_framed_message(stringf("Building %s", pkg_for_log(p).c_str()));
        clone_helper.report();

        log_info("Reason: %s", build_reasons.front().c_str());
        for (int i = 1; i < build_reasons.size(); ++i)
            log_info("%s", build_reasons[i].c_str());

        if (request_eval.status != pkg_request_not_installed && !incremental_install) {
            CHECK(!request_eval.pkg_desc.b.configs.empty());
            log_info("Uninstalling previously installed configurations (%s)",
                join(replace_empty_config(request_eval.pkg_desc.b.configs), ", ").c_str());
            installdb.uninstall(p);
        }

        auto& pd = wsp.pkg_map[p].planned_desc;
        CHECK(!pd.b.configs.empty(),
            "Internal error: at this point there must be at least one explicit configuration "
            "specified to build.");

        cmakex_config_t cfg(binary_dir);
        CHECK(cfg.cmakex_cache().valid);

        bool force_config_step_now = force_config_step;
        for (auto& config : wp.planned_desc.b.configs) {
            build(binary_dir, pd.name, pd.b.source_dir, pd.b.cmake_args, config, { "", "install" },
                force_config_step);
            // for a multiconfig generator we're forcing cmake-config step only for the first
            // configuration. Subsequent configurations share the same binary dir and fed with the
            // same cmake args.
            // for single-config generator it's either single-bin-dir (needs to force each config)
            // or per-config-bin-dir (also needs to force each config)
            if (cfg.cmakex_cache().multiconfig_generator)
                force_config_step_now = false;
            // copy or link installed files into install prefix
            // register this build with installdb
            installed_config_desc_t desc(pd.name, config);
            desc.c = pd.c;
            desc.b.source_dir = pd.b.source_dir;
            desc.b.cmake_args = pd.b.cmake_args;
            desc.deps_shas = pd.deps_shas;
            installdb.install_with_unspecified_files(desc);
        }
        log_info("");
    } // iterate over build order
#endif
}
}
