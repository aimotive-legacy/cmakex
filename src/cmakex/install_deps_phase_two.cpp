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
                            bool force_config_step,
                            const vector<string>& build_args,
                            const vector<string>& native_tool_args)
{
    log_info();
    InstallDB installdb(binary_dir);
    for (auto& p : wsp.build_order) {
        auto& wp = wsp.pkg_map.at(p);
        log_info_framed_message(stringf("Building %s", pkg_for_log(p).c_str()));
        log_info("Checked out @ %s", wp.resolved_git_tag.c_str());
        std::vector<config_name_t> configs_to_build;
        for (auto& kv : wp.pcd) {
            if (!kv.second.build_reasons.empty())
                configs_to_build.emplace_back(kv.first);
        }
        CHECK(!configs_to_build.empty(),
              "Internal error: at this point there must be at least one explicit configuration "
              "specified to build.");
        if (configs_to_build.size() > 1)
            log_info("Configurations: [%s]",
                     join(get_prefer_NoConfig(configs_to_build), ", ").c_str());

        cmakex_config_t cfg(binary_dir);
        CHECK(cfg.cmakex_cache().valid);

        clone_helper_t clone_helper(binary_dir, p);

        bool force_config_step_now = force_config_step;
        for (auto& config : configs_to_build) {
            {
                auto& br = wp.pcd.at(config).build_reasons;
                auto it = br.begin();
                CHECK(it != br.end());
                string s1 = stringf("Reason: ");
                log_info();
                log_info("Building '%s'", config.get_prefer_NoConfig().c_str());
                log_info("%s%s", s1.c_str(), it->c_str());
                s1.assign(s1.size(), ' ');
                for (++it; it != br.end(); ++it)
                    log_info("%s%s", s1.c_str(), it->c_str());
            }
            build(binary_dir, p, wp.request.b.source_dir, wp.pcd.at(config).cmake_args_to_apply,
                  config, {"", "install"}, force_config_step_now, cfg.cmakex_cache(), build_args,
                  native_tool_args);
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
            desc.final_cmake_args = wp.pcd.at(config).tentative_final_cmake_args;
            for (auto& d : wp.request.depends) {
                auto dep_installed = installdb.try_get_installed_pkg_all_configs(d);
                for (auto& kv : dep_installed.config_descs)
                    desc.deps_shas[d][kv.first] = calc_sha(kv.second);
            }
            installdb.install_with_unspecified_files(desc);
        }
        log_info();
    }  // iterate over build order
}
}
