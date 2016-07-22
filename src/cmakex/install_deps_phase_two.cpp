#include "adasworks/sx/check.h"
#include "adasworks/sx/log.h"
#include "build.h"
#include "clone.h"
#include "git.h"
#include "install_deps_phase_two.h"
#include "misc_utils.h"
#include "print.h"

namespace cmakex {

void install_deps_phase_two(string_par binary_dir, deps_recursion_wsp_t& wsp)
{
    InstallDB installdb(binary_dir);
    for (auto& p : wsp.build_order) {
        string build_reason;
        auto& wp = wsp.pkg_map[p];
        auto request_eval = installdb.evaluate_pkg_request(wp.planned_desc);

        // record each dependency's current state and also remember changed deps along the way
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
            wp.planned_desc.dep_shas[d] = dep_current_hash;
        }

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
            } else if (request_eval.pkg_desc.c.git_tag.empty()) {
                build_reason = "initial build";
            } else if (clone_helper.cloned_sha != request_eval.pkg_desc.c.git_tag) {
                build_reason = "workspace contains new commit";
            }
        }

        // check if we need to build because of changed dependencies
        if (build_reason.empty()) {
            // check if we still need to build it because of changed dependencies
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

        if (request_eval.status != pkg_request_not_installed) {
            CHECK(!request_eval.pkg_desc.b.configs.empty());
            log_info("Uninstalling previously installed configurations (%s)",
                     join(request_eval.pkg_desc.b.configs, ", ").c_str());
            installdb.uninstall(p);
        }

        auto& pd = wsp.pkg_map[p].planned_desc;
        CHECK(!pd.b.configs.empty(),
              "Internal error: at this point there must be at least one explicit configuration "
              "specified to build.");
        for (auto& config : wp.planned_desc.b.configs) {
            build(binary_dir, pd, config);
            // copy or link installed files into install prefix
            // register this build with installdb
            installdb.install(pd);
        }
    }  // iterate over build order
}
}
