#include <nowide/args.hpp>

#include <cmath>

#include <nowide/cstdlib.hpp>
#include <nowide/cstdio.hpp>

#include <Poco/Util/Application.h>

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "git.h"
#include "helper_cmake_project.h"
#include "install_deps_phase_one.h"
#include "install_deps_phase_two.h"
#include "misc_utils.h"
#include "print.h"
#include "process_command_line.h"
#include "run_cmake_steps.h"
#include "cmakex_utils.h"

namespace cmakex {

const char* default_cmakex_preset_filename()
{
    return "default-cmakex-presets.yaml";
}

string get_executable_path(string_par argv0)
{
    class DummyApplication : public Poco::Util::Application
    {
    } da;
    da.init({argv0.c_str()});
    return da.commandPath();
}

namespace fs = filesystem;

int main(int argc, char* argv[])
{
    nowide::args nwa(argc, argv);

    log::Logger global_logger(adasworks::log::global_tag);
    LOG_DEBUG("Debug log messages are enabled");
    LOG_TRACE("Trace log messages are enabled");

    {
        auto clg = nowide::getenv("CMAKEX_LOG_GIT");
        if (clg)
            g_log_git = eval_cmake_boolean_or_fail(clg);
    }
    {
        // using default-cmakex-preset.yaml in the application's dir, if needed
        auto exe_path = fs::path(get_executable_path(argv[0])).parent_path().string();
        auto dpf = exe_path + "/" + default_cmakex_preset_filename();
        auto env_cpf = nowide::getenv("CMAKEX_PRESET_FILE");
        if (env_cpf && strlen(env_cpf) == 0)
            env_cpf = 0;
        auto dpf_exists = fs::is_regular_file(dpf);
        auto cpf_exists = env_cpf && fs::is_regular_file(env_cpf);

        if (env_cpf) {
            if (cpf_exists) {
                LOG_DEBUG("Using CMAKEX_PRESET_FILE=%s as default preset file.",
                          path_for_log(env_cpf).c_str());
            } else {
                LOG_WARN(
                    "Would use CMAKEX_PRESET_FILE=%s as default preset file but it doesn't exist.",
                    path_for_log(env_cpf).c_str());
                if (dpf_exists)
                    LOG_WARN(
                        "If CMAKEX_PRESET_FILE were not set, would use %s as default preset file "
                        "(from the directory of the executable) which does exist.",
                        path_for_log(dpf).c_str());
            }
        } else {
            nowide::setenv("CMAKEX_PRESET_FILE", dpf.c_str(), 1);
            if (dpf_exists)
                LOG_DEBUG("Using %s as default preset file (from the directory of the executable).",
                          path_for_log(dpf).c_str());
            else
                LOG_DEBUG(
                    "Would use use %s as default preset file (from the directory of the "
                    "executable) but it doesn't exist.",
                    path_for_log(dpf).c_str());
        }
    }
    int result = EXIT_SUCCESS;

    for (int argix = 1; argix < argc; ++argix) {
        string arg = argv[argix];
        if (arg == "-V")
            g_verbose = true;
    }

    if (g_verbose) {
        log_info("Current directory: %s", path_for_log(fs::current_path().c_str()).c_str());
        for (int i = 0; i < argc; ++i) {
            log_info("arg#%d: `%s`", i, argv[i]);
        }
    }

    auto env_cmake_prefix_path = nowide::getenv("CMAKE_PREFIX_PATH");
    maybe<vector<string>> maybe_env_cmake_prefix_path_vector;
    if (env_cmake_prefix_path) {
        // split along os-specific separator
        vector<pair<string, bool>> msgs;
        maybe_env_cmake_prefix_path_vector =
            cmakex_prefix_path_to_vector(env_cmake_prefix_path, true);
    }

    FILE* manifest_handle = nullptr;

    try {
        auto cla = process_command_line_1(argc, argv);
        if (cla.subcommand.empty())
            exit(EXIT_SUCCESS);
        processed_command_line_args_cmake_mode_t pars;
        cmakex_cache_t cmakex_cache;

        tie(pars, cmakex_cache) = process_command_line_2(cla);

        // cmakex_cache may contain new data to the stored cmakex_cache, or
        // in case of a first cmakex call on this binary dir, it is not saved at all
        // We'll save it on the first successful configuration: either after configuring the
        // wrapper
        // project or after configuring the main project

        CHECK(pars.deps_mode == dm_deps_only || !pars.source_dir.empty());

        if (!pars.manifest.empty()) {
            manifest_handle = nowide::fopen(pars.manifest.c_str(), "w");
            if (!manifest_handle)
                throw runtime_error(stringf("Can't open manifest file for writing: %s",
                                            path_for_log(pars.manifest).c_str()));
        }

        if (pars.deps_mode != dm_main_only) {
            deps_recursion_wsp_t wsp;
            wsp.force_build = pars.force_build;
            wsp.clear_downloaded_include_files = pars.clear_downloaded_include_files;
            // command_line_cmake_args contains the cmake args specified in the original cmakex call
            // except any arg dealing with CMAKE_INSTALL_PREFIX
            wsp.update = pars.update_mode != update_mode_none;
            wsp.update_can_leave_branch = pars.update_mode == update_mode_if_clean ||
                                          pars.update_mode == update_mode_all_clean;
            wsp.update_stop_on_error = pars.update_mode == update_mode_all_clean ||
                                       pars.update_mode == update_mode_all_very_clean;
            vector<string> command_line_cmake_args;
            for (auto& c : pars.cmake_args) {
                auto pca = parse_cmake_arg(c);
                if (pca.name != "CMAKE_INSTALL_PREFIX") {
                    command_line_cmake_args.emplace_back(c);
                }
            }
            command_line_cmake_args = normalize_cmake_args(command_line_cmake_args);

            vector<config_name_t> configs;
            configs.reserve(pars.configs.size());
            for (auto& c : pars.configs)
                configs.emplace_back(c);

            log_info("Configuring helper CMake project");
            HelperCmakeProject hcp(pars.binary_dir);
            hcp.configure(command_line_cmake_args, "_cmakex");

            // get some variables from helper project cmake cache
            auto& cc = hcp.cmake_cache;

            // remember CMAKE_ROOT
            if (cc.vars.count("CMAKE_ROOT") > 0)
                cmakex_cache.cmake_root = cc.vars.at("CMAKE_ROOT");

            // remember CMAKE_PREFIX_PATH
            if (cc.vars.count("CMAKE_PREFIX_PATH") > 0) {
                cmakex_cache.cmakex_prefix_path_vector =
                    cmakex_prefix_path_to_vector(cc.vars.at("CMAKE_PREFIX_PATH"), false);
            } else
                cmakex_cache.cmakex_prefix_path_vector.clear();

            // remember ENV{CMAKE_PREFIX_PATH}
            if (maybe_env_cmake_prefix_path_vector)
                cmakex_cache.env_cmakex_prefix_path_vector = *maybe_env_cmake_prefix_path_vector;

            // after successful configuration of the helper project the cmake generator setting has
            // been validated and fixed so we're writing out the cmakex cache
            write_cmakex_cache_if_dirty(pars.binary_dir, cmakex_cache);

            fs::create_directories(cmakex_config_t(pars.binary_dir).find_module_hijack_dir());

            string ds = pars.deps_script;
            if (!ds.empty() && fs::is_regular_file(ds))
                ds = fs::lexically_normal(fs::absolute(ds));
            install_deps_phase_one(pars.binary_dir, pars.source_dir, {}, command_line_cmake_args,
                                   configs, wsp, cmakex_cache, ds);
#if 0
            for (auto& kv : wsp.pkg_map) {
                auto& pkg_name = kv.first;
                auto& pkg = kv.second;
                LOG_INFO(" *** %s ***", pkg_for_log(pkg_name).c_str());
                LOG_INFO("deps from script: %s", pkg.dependencies_from_script ? "yes" : "no");
                for (auto& kvx : pkg.pcd) {
                    LOG_INFO("- config: %s", kvx.first.get_prefer_NoConfig().c_str());
                    LOG_INFO("- CMAKE_ARGS: %s",
                             join(kvx.second.tentative_final_cmake_args.args, ", ").c_str());
                }
                LOG_INFO("SOURCE_DIR %s", pkg.request.b.source_dir.c_str());
                LOG_INFO("GIT_TAG %s -> %s", pkg.request.c.git_tag.c_str(),
                         pkg.resolved_git_tag.c_str());
                LOG_INFO("GIT_URL %s", pkg.request.c.git_url.c_str());
                LOG_INFO("DEPENDS %s", join(pkg.request.depends, ", ").c_str());
            }
#endif
            install_deps_phase_two(pars.binary_dir, wsp, !pars.cmake_args.empty() || pars.flag_c,
                                   pars.build_args, pars.native_tool_args);
            log_info("%d dependenc%s %s been processed.", (int)wsp.pkg_map.size(),
                     wsp.pkg_map.size() == 1 ? "y" : "ies",
                     wsp.pkg_map.size() == 1 ? "has" : "have");
            log_info();
            if (manifest_handle) {
                fprintf(manifest_handle, "#### DEPENDENCIES ####\n\n");
                for (auto& kv : wsp.pkg_map) {
                    auto pkg_name = kv.first;
                    auto& mspc = kv.second.manifests_per_config;
                    if (mspc.empty())
                        continue;
                    auto it = mspc.begin();
                    auto& m0 = it->second;
                    bool all_the_same = true;
                    for (; it != mspc.end(); ++it) {
                        auto& m = it->second;
                        if (m != m0) {
                            all_the_same = false;
                            break;
                        }
                    }

                    auto printf_add_pkg = [&pkg_name](const manifest_of_config_t& m0, string prefix,
                                                      string configs) {
                        string ap = prefix + stringf("add_pkg(%s", pkg_name.c_str());
                        if (!m0.git_url.empty())
                            ap += " " + m0.git_url;
                        if (!m0.git_tag_and_comment.empty())
                            ap += " " + m0.git_tag_and_comment;
                        ap += "\n";
                        if (!m0.source_dir.empty())
                            ap += prefix + "    " + m0.source_dir + "\n";
                        if (!m0.cmake_args.empty())
                            ap += prefix + "    " + m0.cmake_args + "\n";
                        if (!m0.depends_maybe_commented.empty())
                            ap += prefix + "    " + m0.depends_maybe_commented + "\n";
                        ap += prefix + "    " + configs + ")\n";
                        return ap;
                    };

                    if (all_the_same) {
                        string configs =
                            stringf("CONFIGS %s",
                                    join(get_prefer_NoConfig(keys_of_map(mspc)), " ").c_str());
                        auto s = printf_add_pkg(m0, "", configs);
                        fprintf(manifest_handle, "%s", s.c_str());
                    } else {
                        auto it = mspc.begin();
                        auto s = printf_add_pkg(
                            it->second, "",
                            stringf("CONFIGS %s", it->first.get_prefer_NoConfig().c_str()));
                        fprintf(manifest_handle, "%s", s.c_str());
                        fprintf(manifest_handle,
                                "# The following configurations has also been installed but with "
                                "different build settings:");
                        for (++it; it != mspc.end(); ++it) {
                            auto s = printf_add_pkg(
                                it->second, "# ",
                                stringf("CONFIGS %s", it->first.get_prefer_NoConfig().c_str()));
                            fprintf(manifest_handle, "%s", s.c_str());
                        }
                    }
                    fprintf(manifest_handle, "\n");
                }
                fprintf(manifest_handle, "\n");
            }
        }
        if (pars.deps_mode == dm_deps_and_main)
            log_info_framed_message(stringf("Building main project"));

        if (pars.deps_mode != dm_deps_only) {
            run_cmake_steps(pars, cmakex_cache);
            if (manifest_handle) {
                string report = "#### MAIN PROJECT ####\n#\n";
                report += stringf("# current directory: %s\n", fs::current_path().c_str());
                report += "# command line:\n#     ";
                string command_line;
                for (int i = 0; i < argc; ++i) {
                    if (!command_line.empty())
                        command_line += " ";
                    command_line += escape_command_line_arg(argv[i]);
                }
                report += stringf("%s\n#\n", command_line.c_str());

                string abs_source_dir = fs::absolute(pars.source_dir).string();

                string remote_lines = "# git remote -v\n";
                try {
                    OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
                    int r = exec_git({"remote", "-v"}, abs_source_dir, oeb.stdout_callback(),
                                     nullptr, log_git_command_on_error);
                    if (r)
                        throw std::runtime_error(
                            stringf("'git remote -v' failed with error code %d", r));
                    OutErrMessages oem(oeb.move_result());
                    for (int i = 0; i < oem.size(); ++i) {
                        auto msg = oem.at(i);
                        auto lines = split_at_newlines(msg.text);
                        for (auto& l : lines)
                            remote_lines += stringf("#     %s\n", l.c_str());
                    }
                } catch (const exception& e) {
                    remote_lines = stringf("# Can't get git remote, reason: %s\n", e.what());
                } catch (...) {
                    remote_lines = "# Can't get git remote, reason: Unknown exception.\n";
                }
                report += remote_lines;

                string rev_parse_result;
                try {
                    auto sha = git_rev_parse("HEAD", abs_source_dir);
                    if (sha.empty())
                        rev_parse_result = stringf(
                            "# Can't get git revision, source dir is probably not a git dir: %s",
                            path_for_log(abs_source_dir).c_str());
                    else
                        rev_parse_result = stringf("# HEAD is at %s", sha.c_str());
                } catch (const exception& e) {
                    rev_parse_result = stringf("# Can't get git revision, reason: %s", e.what());
                } catch (...) {
                    rev_parse_result =
                        stringf("# Can't get git revision, reason: Unknown exception.");
                }
                report += stringf("%s\n", rev_parse_result.c_str());

                string status_lines = "# git status -sb\n";
                try {
                    auto y = git_status(abs_source_dir, true);
                    for (auto& l : y.lines) {
                        status_lines += stringf("#     %s\n", l.c_str());
                    }
                } catch (const exception& e) {
                    status_lines = stringf("# Can't get git status, reason: %s\n", e.what());
                } catch (...) {
                    status_lines = "# Can't get git status, reason: Unknown exception.\n";
                }
                report += status_lines;

                fprintf(manifest_handle, "%s", report.c_str());
            }
        }
        if (manifest_handle)
            log_info("Manifest file written to: %s", path_for_log(pars.manifest).c_str());
    } catch (const exception& e) {
        log_fatal("%s", e.what());
        result = EXIT_FAILURE;
    } catch (...) {
        log_fatal("Unhandled exception");
        result = EXIT_FAILURE;
    }

    if (manifest_handle)
        fclose(manifest_handle);

    return result;
}
}

int main(int argc, char* argv[])
{
    return cmakex::main(argc, argv);
}
