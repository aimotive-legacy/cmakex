#include "install_deps_phase_two.h"

#include "adasworks/sx/check.h"
#include "adasworks/sx/log.h"

#include "build.h"
#include "clone.h"
#include "cmakex_utils.h"
#include "git.h"
#include "misc_utils.h"
#include "print.h"

namespace cmakex {

void install_deps_phase_two(string_par binary_dir, deps_recursion_wsp_t& wsp)
{
    log_info("");
    InstallDB installdb(binary_dir);
    for (auto& p : wsp.build_order) {
        vector<string> build_reasons;
        auto& wp = wsp.pkg_map[p];
        auto request_eval = installdb.evaluate_pkg_request(wp.planned_desc);

        // record each dependency's current state and also remember changed deps along the way
        vector<string> deps_changed;
        for (auto& d : wp.planned_desc.depends) {
            // check d's original and current installation statuses
            auto maybe_current_desc = installdb.try_get_installed_pkg_desc(d);
            CHECK(maybe_current_desc,
                  "Internal error: a dependency %s was not installed at the time the "
                  "%s was about to build",
                  pkg_for_log(d).c_str(), pkg_for_log(p).c_str());
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
                build_reasons = {"workspace contains uncommited changes"};
            } else if (request_eval.pkg_desc.c.git_tag.empty()) {
                build_reasons = {"initial build"};
            } else if (clone_helper.cloned_sha != request_eval.pkg_desc.c.git_tag) {
                build_reasons = {"workspace contains new commit"};
            }
        }

        // check if we need to build because of changed dependencies
        if (build_reasons.empty()) {
            // check if we still need to build it because of changed dependencies
            if (!deps_changed.empty())
                build_reasons = {stringf("dependencies changed since last build: %s",
                                         join(deps_changed, ", ").c_str())};
        }

        // check if we need to build because of some other reason, decided in phase one
        if (build_reasons.empty()) {
            switch (request_eval.status) {
                case pkg_request_satisfied:
                    break;
                case pkg_request_missing_configs:
                    build_reasons = {stringf("missing configurations (%s)",
                                             join(request_eval.missing_configs, ", ").c_str())};
                    break;
                case pkg_request_not_installed:
                    build_reasons = {"initial build"};
                    break;
                case pkg_request_not_compatible:
                    build_reasons = {stringf("build options changed")};
                    build_reasons.emplace_back(
                        stringf("CMAKE_ARGS of the currently installed build: %s",
                                join(make_canonical_cmake_args(wp.planned_desc.b.cmake_args), " ")
                                    .c_str()));
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

        auto msg = stringf("* Building %s *", pkg_for_log(p).c_str());
        auto stars = string(msg.size(), '*');
        log_info("%s", stars.c_str());
        log_info("%s", msg.c_str());
        log_info("%s", stars.c_str());

        log_info("Reason: %s", build_reasons.front().c_str());
        for (int i = 1; i < build_reasons.size(); ++i)
            log_info("%s", build_reasons[i].c_str());

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
        log_info("");
    }  // iterate over build order
}
}
