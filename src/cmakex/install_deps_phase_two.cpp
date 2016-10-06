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
    cmakex_config_t cfg(binary_dir);
    CHECK(cfg.cmakex_cache().valid);
    auto prefix_paths = stable_unique(concat(cfg.cmakex_cache().cmakex_prefix_path_vector,
                                             cfg.cmakex_cache().env_cmakex_prefix_path_vector));

    auto create_desc = [&installdb, &prefix_paths](
        const string& p, config_name_t config, const deps_recursion_wsp_t::pkg_t& wp,
        const vector<string>& hijack_modules_needed, const string& cloned_sha) {
        installed_config_desc_t desc(p, config);
        desc.git_url = wp.request.c.git_url;
        if (cloned_sha == k_sha_uncommitted) {
            desc.git_sha = stringf("<installed-from-uncommited-changes-at-%s",
                                   current_datetime_string_for_log().c_str());
        } else {
            desc.git_sha = wp.resolved_git_tag;
        }
        desc.source_dir = wp.request.b.source_dir;
        LOG_TRACE("1 %s", config.get_prefer_NoConfig().c_str());
        desc.final_cmake_args = wp.pcd.at(config).tentative_final_cmake_args;
        LOG_TRACE("2");
        for (auto& d : wp.request.depends) {
            auto dep_installed = installdb.try_get_installed_pkg_all_configs(d, prefix_paths);
            for (auto& kv : dep_installed.config_descs)
                desc.deps_shas[d][kv.first] = calc_sha(kv.second);
        }
        desc.hijack_modules_needed = hijack_modules_needed;
        return desc;
    };

    auto create_moc = [](const deps_recursion_wsp_t::pkg_t& wp, const string& git_url,
                         const string& git_sha, const vector<string>& depends,
                         const string& source_dir, const final_cmake_args_t& final_cmake_args) {
        manifest_of_config_t moc;
        moc.git_url = stringf("GIT_URL %s", git_url.c_str());
        moc.git_tag = stringf("GIT_TAG %s", git_sha.c_str());
        moc.git_tag_and_comment =
            stringf("GIT_TAG %s # request: %s", git_sha.c_str(),
                    wp.request.c.git_tag.empty() ? "HEAD" : wp.request.c.git_tag.c_str());
        if (!depends.empty()) {
            string depends_list = join(depends, " ");
            if (wp.dependencies_from_script)
                moc.depends_maybe_commented =
                    stringf("# dependencies defined in deps.cmake: %s", depends_list.c_str());
            else
                moc.depends_maybe_commented = stringf("DEPENDS %s", depends_list.c_str());
        }
        if (!source_dir.empty())
            moc.source_dir = stringf("SOURCE %s", path_for_log(source_dir).c_str());
        if (!final_cmake_args.args.empty()) {
            moc.cmake_args = "CMAKE_ARGS";
            for (auto& ca : final_cmake_args.args) {
                auto pca = parse_cmake_arg(ca);
                if (pca.name == "CMAKE_PREFIX_PATH" || pca.name == "CMAKE_MODULE_PATH" ||
                    pca.name == "CMAKE_INSTALL_PREFIX" || pca.name == "CMAKE_TOOLCHAIN_FILE")
                    continue;
                moc.cmake_args += " ";
                moc.cmake_args += escape_cmake_arg(ca);
            }
        }
        moc.c_sha = final_cmake_args.c_sha;
        moc.toolchain_sha = final_cmake_args.cmake_toolchain_file_sha;
        return moc;
    };
    for (auto& p : wsp.build_order) {
        // pkgs_to_moc.erase(p);
        log_datetime();
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
            auto build_result =
                build(binary_dir, p, wp.request.b.source_dir, wp.pcd.at(config).cmake_args_to_apply,
                      config, {"", "install"}, force_config_step_now, cfg.cmakex_cache(),
                      build_args, native_tool_args);

            for (auto& base : build_result.hijack_modules_needed)
                write_hijack_module(base, binary_dir);

            // for a multiconfig generator we're forcing cmake-config step only for the first
            // configuration. Subsequent configurations share the same binary dir and fed with the
            // same cmake args.
            // for single-config generator it's either single-bin-dir (needs to force each config)
            // or per-config-bin-dir (also needs to force each config)
            if (cfg.cmakex_cache().multiconfig_generator)
                force_config_step_now = false;
            // copy or link installed files into install prefix
            // register this build with installdb
            CHECK(clone_helper.cloned);
            auto desc = create_desc(p, config, wp, build_result.hijack_modules_needed,
                                    clone_helper.cloned_sha);

            /*
                        auto moc = create_moc(desc, wp);
                        wp.manifests_per_config.insert(std::make_pair(config, move(moc)));
            */
            installdb.install_with_unspecified_files(desc);
        }
        log_info();
    }  // iterate over build order
    for (auto& kv : wsp.pkg_map) {
        auto& p = kv.first;
        auto& wp = kv.second;
        auto ics = installdb.try_get_installed_pkg_all_configs(p, prefix_paths);
        for (auto& ic : ics.config_descs) {
            auto& config = ic.first;
            LOG_TRACE("Creating moc for %s - %s", p.c_str(), config.get_prefer_NoConfig().c_str());
            auto& ics = ic.second;
            auto moc = create_moc(wp, ics.git_url, ics.git_sha, keys_of_map(ics.deps_shas),
                                  ics.source_dir, ics.final_cmake_args);
            wp.manifests_per_config.insert(std::make_pair(config, move(moc)));
        }
    }
}
}
