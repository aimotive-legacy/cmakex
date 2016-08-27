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
        CHECK(!wp.build_reasons.empty(),
              "Internal error: at this point there must be at least one explicit configuration "
              "specified to build.");
        if (wp.build_reasons.size() > 1)
            log_info("Configurations: [%s]",
                     join(get_prefer_NoConfig(keys_of_map(wp.build_reasons)), ", ").c_str());

        cmakex_config_t cfg(binary_dir);
        CHECK(cfg.cmakex_cache().valid);

        clone_helper_t clone_helper(binary_dir, p);

        bool force_config_step_now = force_config_step;
        for (auto& kv : wp.build_reasons) {
            auto& config = kv.first;
            {
                auto it = kv.second.begin();
                CHECK(it != kv.second.end());
                string s1 =
                    stringf("Building '%s', reason: ", config.get_prefer_NoConfig().c_str());
                log_info("%s%s", s1.c_str(), it->c_str());
                s1.assign(s1.size(), ' ');
                for (++it; it != kv.second.end(); ++it)
                    log_info("%s%s", s1.c_str(), it->c_str());
            }
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
            desc.git_url = wp.request.c.git_url;
            CHECK(clone_helper.cloned);
            if (clone_helper.cloned_sha == k_sha_uncommitted) {
                desc.git_sha = stringf("<installed-from-uncommited-changes-at-%s",
                                       current_datetime_string_for_log().c_str());
            } else {
                desc.git_sha = wp.resolved_git_tag;
            }

            desc.source_dir = wp.request.b.source_dir;
            desc.final_cmake_args = wp.final_cmake_args;
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
