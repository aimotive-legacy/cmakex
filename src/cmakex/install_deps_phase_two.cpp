#include "adasworks/sx/check.h"
#include "adasworks/sx/log.h"
#include "build.h"
#include "clone.h"
#include "git.h"
#include "install_deps_phase_two.h"
#include "misc_utils.h"
#include "print.h"

namespace cmakex {

void install(const pkg_desc_t& pkg_request)
{
    LOG_INFO("Installing %s", pkg_request.name.c_str());
    // todo
}

void install_deps_phase_two(string_par binary_dir, deps_recursion_wsp_t& wsp)
{
    InstallDB installdb(binary_dir);
    for (auto& p : wsp.build_order) {
        string build_reason;
        auto& wp = wsp.pkg_map[p];
        auto request_eval = installdb.evaluate_pkg_request(wp.planned_desc);

        // the default is that if we need to build, we need to build all configurations
        auto configs_to_build = wp.planned_desc.b.configs;
        // the default is to uninstall previously installed files
        bool uninstall = true;

        CHECK(!configs_to_build.empty(),
              "Internal error: at this point there must be at least one explicit configuration "
              "specified to build.");

        clone_helper_t clone_helper(binary_dir, p);

        // check if we still need to build it because of new commits or uncommited changes
        // - from the previous check re.status was pkg_request_satisfied
        // One possible reason we should still rebuild it is that the current workspace
        // has changed compared to what is currently installed.
        // If it were strict-commit mode the phase-one should have failed.
        // So, we simply need to build if the current workspace and installed sha are different.
        if (clone_helper.cloned) {
            if (clone_helper.cloned_sha == k_sha_uncommitted) {
                build_reason = "workspace contains uncommited changes";
            } else if (clone_helper.cloned_sha != request_eval.pkg_desc.c.git_tag) {
                build_reason = "workspace contains new commit";
            }
        }

        // check if we need to build because of changed dependencies
        if (build_reason.empty()) {
            // check if we still need to build it because of changed dependencies
            vector<string> deps_changed;
            for (auto& d : wp.planned_desc.depends) {
                // check d's original and current installation statuses
                auto maybe_current_desc = installdb.try_get_installed_pkg_desc(d);
                CHECK(maybe_current_desc,
                      "Internal error: a dependency '%s' was not installed at the time the "
                      "package '%s' was about to build",
                      d.c_str(), p.c_str());
                auto& dep_current_desc = *maybe_current_desc;
                auto dep_current_hash = dep_current_desc.sha_of_installed_files;
                auto dep_orig_hash = request_eval.pkg_desc.dep_shas.at(d);
                CHECK(!dep_current_hash.empty());
                CHECK(!dep_orig_hash.empty());
                if (dep_current_hash != dep_orig_hash)
                    deps_changed.emplace_back(d);
            }
            if (!deps_changed.empty())
                build_reason = stringf("dependencies changed since last build: %s",
                                       join(deps_changed, ", ").c_str());
        }

        // check if we need to build because of some other reason, decided in phase one
        if (build_reason.empty()) {
            switch (request_eval.status) {
                case pkg_request_satisfied:
                    break;
                case pkg_request_missing_configs:
                    build_reason = stringf("missing configurations (%s)",
                                           join(request_eval.missing_configs, ", ").c_str());
                    configs_to_build = request_eval.missing_configs;
                    uninstall = false;
                    break;
                case pkg_request_not_installed:
                    build_reason = "initial build";
                    break;
                case pkg_request_not_compatible:
                    build_reason = stringf("build options changed (%s)",
                                           request_eval.incompatible_cmake_args.c_str());
                    break;
                default:
                    CHECK(false);
            }
        }

        if (build_reason.empty())
            continue;

        log_info("Building '%s', reason: %s", p.c_str(), build_reason.c_str());

        if (uninstall) {
            log_info("Uninstalling previously installed configurations (%s)",
                     join(request_eval.pkg_desc.b.configs, ", ").c_str());
            installdb.uninstall(p);
        }
        auto& pd = wsp.pkg_map[p].planned_desc;
        for (auto& cfg : configs_to_build) {
            build(binary_dir, pd, cfg);
            // copy or link installed files into install prefix
            // register this build with installdb
        }
    }  // iterate over build order
}
}
