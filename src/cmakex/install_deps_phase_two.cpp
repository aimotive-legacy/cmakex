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
    log_info("");
    InstallDB installdb(binary_dir);
    for (auto& p : wsp.build_order) {
        auto& wp = wsp.pkg_map[p];
        log_info_framed_message(stringf("Building %s", pkg_for_log(p).c_str()));
        log_info("Checked out @ %s", wp.resolved_git_tag.c_str());
        log_info("Configurations and build reasons:");
        int max_config_name = 0;
        CHECK(!wp.build_reasons.empty(),
              "Internal error: at this point there must be at least one explicit configuration "
              "specified to build.");
        for (auto& kv : wp.build_reasons)
            max_config_name = std::max<int>(max_config_name, kv.first.get_prefer_NoConfig().size());
        for (auto& kv : wp.build_reasons) {
            string config_name = kv.first.get_prefer_NoConfig();
            auto it = kv.second.begin();
            CHECK(it != kv.second.end());
            log_info("%s%s: %s", string(max_config_name - config_name.size(), ' ').c_str(),
                     config_name.c_str(), it->c_str());
            for (++it; it != kv.second.end(); ++it)
                log_info("%s%s", string(max_config_name + 2, ' ').c_str(), it->c_str());
        }

        cmakex_config_t cfg(binary_dir);
        CHECK(cfg.cmakex_cache().valid);

        clone_helper_t clone_helper(binary_dir, p);

        bool force_config_step_now = force_config_step;
        for (auto& kv : wp.build_reasons) {
            auto& config = kv.first;
            build(binary_dir, p, wp.request.b.source_dir, wp.final_cmake_args, config,
                  {"", "install"}, force_config_step);
            // for a multiconfig generator we're forcing cmake-config step only for the first
            // configuration. Subsequent configurations share the same binary dir and fed with the
            // same cmake args.
            // for single-config generator it's either single-bin-dir (needs to force each config)
            // or per-config-bin-dir (also needs to force each config)
            if (cfg.cmakex_cache().multiconfig_generator)
                force_config_step_now = false;
            // copy or link installed files into install prefix
            // register this build with installdb
            installed_config_desc_t desc(p, config);
            desc.c.git_url = wp.request.c.git_url;
            CHECK(clone_helper.cloned);
            if (clone_helper.cloned_sha == k_sha_uncommitted) {
                desc.c.git_sha = stringf("<installed-from-uncommited-changes-at-%s",
                                         current_datetime_string_for_log().c_str());
            } else {
                desc.c.git_sha = wp.resolved_git_tag;
            }

            desc.b.source_dir = wp.request.b.source_dir;
            desc.b.cmake_args = wp.request.b.cmake_args;
            for (auto& d : wp.request.depends) {
                auto dep_installed = installdb.try_get_installed_pkg_all_configs(d);
                for (auto& kv : dep_installed.config_descs)
                    desc.deps_shas[d][kv.first] = calc_sha(kv.second);
            }
            installdb.install_with_unspecified_files(desc);
        }
        log_info("");
    }  // iterate over build order
}
}
