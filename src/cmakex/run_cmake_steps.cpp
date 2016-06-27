#include "run_cmake_steps.h"

#include <adasworks/sx/check.h>
#include <adasworks/sx/format.h>

#include "filesystem.h"

#include "misc_utils.h"
#include "out_err_messages.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;
namespace sx = adasworks::sx;

void run_cmake_steps(const cmakex_pars_t& pars)
{
    CHECK(pars.subcommand == cmakex_pars_t::subcommand_cmake_steps);
    CHECK(!pars.source_dir.empty());
    CHECK(!pars.binary_dir.empty());
    string source_dir = pars.source_dir;

    auto main_tic = high_resolution_clock::now();
    log_info("Started at %s", current_datetime_string_for_log().c_str());
    {
        string steps;
        auto add_comma = [&steps]() {
            if (!steps.empty())
                steps += ", ";
        };
        int c = 0;
        if (pars.c) {
            add_comma();
            steps += "configure";
            ++c;
        }
        if (pars.b) {
            add_comma();
            steps += "build";
            ++c;
        }
        if (pars.t) {
            add_comma();
            steps += "test";
            ++c;
        }
        log_info("Performing CMake %s step%s", steps.c_str(), c > 1 ? "s" : "");
    }
    {
        string s = stringf("CMAKE_BINARY_DIR: %s", pars.binary_dir.c_str());
        if (!source_dir.empty())
            s += stringf(", CMAKE_SOURCE_DIR: %s", source_dir.c_str());
        log_info("%s", s.c_str());
    }
    if (!pars.build_targets.empty())
        log_info("Targets: %s", join(pars.build_targets, " ").c_str());
    if (!pars.configs.empty())
        log_info("Configurations: %s", join(pars.configs, " ").c_str());

    // add a single, neutral config, if no configs specified
    auto configs = pars.configs;
    if (configs.empty())
        configs = {""};

    // if need to build but no build targets specified, add a single, default build target
    auto build_targets = pars.build_targets;
    if (pars.b && build_targets.empty())
        build_targets = {""};

    for (auto& config : configs) {
        // configure step
        if (pars.c) {
            auto tic = high_resolution_clock::now();
            string step_string = stringf(
                "configure step%s", config.empty() ? "" : (string(" (") + config + ")").c_str());
            log_info("Begin %s", step_string.c_str());
            vector<string> args;
            if (!source_dir.empty())
                args.emplace_back(string("-H") + source_dir);
            args.emplace_back(string("-B") + pars.binary_dir);
            if (!config.empty())
                args.emplace_back(string("-DCMAKE_BUILD_TYPE=") + config);
            args.insert(args.end(), BEGINEND(pars.config_args));
            log_exec("cmake", args);
            int r = exec_process("cmake", args);
            if (r != EXIT_SUCCESS)
                throwf("Configuring failed with error code %d", r);
            log_info(
                "End %s, elapsed %s", step_string.c_str(),
                sx::format_duration(dur_sec(high_resolution_clock::now() - tic).count()).c_str());
        } else if (pars.config_args_besides_binary_dir) {
            string s =
                "You specified cmake configuration arguments but the 'c' option is missing from "
                "the command word. The specified configuration arguments: ";
            vector<string> a;
            if (!pars.source_dir.empty())
                a.emplace_back(string("-H") + pars.source_dir);
            a.insert(a.end(), BEGINEND(pars.config_args));
            s += join(a, " ");
            throw runtime_error(s);
        }

        // build step
        for (auto& target : build_targets) {
            auto tic = high_resolution_clock::now();
            string step_string =
                stringf("build step for %s%s",
                        target.empty() ? "the default target"
                                       : stringf("target '%s'", target.c_str()).c_str(),
                        config.empty() ? "" : (string(" (") + config + ")").c_str());
            log_info("Begin %s", step_string.c_str());
            vector<string> args = {string("--build")};
            CHECK(!pars.binary_dir.empty());
            args.emplace_back(pars.binary_dir);
            args.insert(args.end(), BEGINEND(pars.build_args));
            if (!target.empty()) {
                args.emplace_back("--target");
                args.emplace_back(target);
            }
            if (!config.empty()) {
                args.emplace_back("--config");
                args.emplace_back(config);
            }
            args.insert(args.end(), BEGINEND(pars.native_tool_args));
            log_exec("cmake", args);
            int r = exec_process("cmake", args);
            if (r != EXIT_SUCCESS)
                throwf("Building failed with error code %d", r);
            log_info(
                "End %s, elapsed %s", step_string.c_str(),
                sx::format_duration(dur_sec(high_resolution_clock::now() - tic).count()).c_str());
        }

        // test step
        if (pars.t) {
            auto tic = high_resolution_clock::now();
            string step_string =
                stringf("test step%s", config.empty() ? "" : (string(" (") + config + ")").c_str());
            log_info("Begin %s", step_string.c_str());
            fs::current_path(pars.binary_dir);
            vector<string> args;
            if (!config.empty()) {
                args.emplace_back("-C");
                args.emplace_back(config);
            }
            log_exec("ctest", args);
            int r = exec_process("ctest", args);
            if (r != EXIT_SUCCESS)
                throwf("Testing failed with error code %d", r);
            log_info(
                "End %s, elapsed %s", step_string.c_str(),
                sx::format_duration(dur_sec(high_resolution_clock::now() - tic).count()).c_str());
        }
    }  // for configs
    log_info("Finished at %s, elapsed %s", current_datetime_string_for_log().c_str(),
             sx::format_duration(dur_sec(high_resolution_clock::now() - main_tic).count()).c_str());
}
}
