#include "run_build_script.h"

#include <adasworks/sx/check.h>

#include "exec_process.h"
#include "filesystem.h"
#include "misc_util.h"
#include "out_err_messages.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;

const char* k_build_script_runner_subdir = "_cmakex/build_script_runner_project";
const char* k_log_subdir = "_cmakex/log";
const char* k_default_binary_dirname = "b";
const char* k_runner_project_command_cache_var = "__CMAKEX_RUNNER_PROJECT_COMMAND";
const char* k_runner_project_command_var = "_CMAKEX_RUNNER_PROJECT_COMMAND";
const char* k_build_script_runner_log_name = "build_script_runner";
const char* k_log_extension = ".log";

string build_script_runner_cmakelists()
{
    return stringf(
               "cmake_minimum_required(VERSION ${CMAKE_VERSION}\n\n"
               "if(DEFINED %s)\n"
               "  set(%s \"${%s}\")\n"
               "  unset(%s CACHE)\n"
               "endif()\n\n",
               k_runner_project_command_cache_var, k_runner_project_command_var,
               k_runner_project_command_cache_var, k_runner_project_command_cache_var) +
           stringf(
               "if(DEFINED %s)\n"
               "  message(STATUS \"Build script runner command: ${%s}\")\n"
               "endif()\n\n",
               k_runner_project_command_var, k_runner_project_command_var);
}
void run_build_script(const cmakex_pars_t& pars)
{
    // Create background cmake project
    // Configure it again with specifying the build script as parameter
    // The background project executes the build script and
    // - records the add_pkg commands
    // - records the cmakex commands
    // Then the configure ends.
    // Process the recorded add_pkg commands. The result of an add_pkg command is
    // the install directory of the project symlinked or copied into the local
    // prefix dir.

    // create source dir
    CHECK(!pars.binary_dir.empty());
    CHECK(pars.source_desc_kind == source_descriptor_build_script);

    print_out("Running build script \"%s\"", pars.source_desc.c_str());
    string build_script_runner_source_dir = pars.binary_dir + "/" + k_build_script_runner_subdir;
    string build_script_runner_binary_dir =
        build_script_runner_source_dir + "/" + k_default_binary_dirname;

    if (!fs::is_directory(build_script_runner_source_dir)) {
        string msg;
        try {
            fs::create_directories(build_script_runner_source_dir);
        } catch (const exception& e) {
            msg = e.what();

        } catch (...) {
            msg = "unknown exception";
        }
        if (!msg.empty()) {
            print_err("Can't create directory \"%s\", reason: %s.",
                      build_script_runner_source_dir.c_str(), msg.c_str());
            exit(EXIT_FAILURE);
        }
    }

    string cmakelists_path = build_script_runner_source_dir + "/CMakeLists.txt";
    if (!fs::exists(cmakelists_path)) {
        FILE* f = fopen(cmakelists_path.c_str(), "wt");
        if (!f) {
            print_err("Can't open \"%s\" for writing", cmakelists_path.c_str());
            exit(EXIT_FAILURE);
        }
        int r = fprintf(f, "%s\n", build_script_runner_cmakelists().c_str());
        int was_errno = errno;
        fclose(f);
        if (r < 0) {
            print_err("Write error for \"%s\", reason: (%d) %s", cmakelists_path.c_str(), was_errno,
                      strerror(was_errno));
            exit(EXIT_FAILURE);
        }
    }

    vector<string> args;
    args.emplace_back(string("-H") + build_script_runner_source_dir);
    args.emplace_back(string("-B") + build_script_runner_binary_dir);

    args.insert(args.end(), BEGINEND(pars.config_args));
    args.emplace_back(string("-U") + k_runner_project_command_cache_var);
    print_out("Configuring build script runner project.");
    log_exec("cmake", args);
    OutErrMessagesBuilder oeb(false);
    int r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
    auto oem = oeb.move_result();

    string log_dir = pars.binary_dir + "/" + k_log_subdir;
    save_log_from_oem(oem, log_dir,
                      string(k_build_script_runner_log_name) + "-configure" + k_log_extension);

    if (r != EXIT_SUCCESS) {
        print_err("Failed configuring build script runner project, result: %d.", r);
        exit(EXIT_FAILURE);
    }

    args.clear();
    args.emplace_back(build_script_runner_binary_dir);
    args.emplace_back(string("-D") + k_runner_project_command_cache_var + "=run;" +
                      pars.source_desc);

    print_out("Executing build script by runner project.");
    log_exec("cmake", args);
    OutErrMessagesBuilder oeb2(false);
    r = exec_process("cmake", args, oeb2.stdout_callback(), oeb2.stderr_callback());
    auto oem2 = oeb2.move_result();
    save_log_from_oem(oem2, log_dir,
                      string(k_build_script_runner_log_name) + "-run" + k_log_extension);
    if (r != EXIT_SUCCESS) {
        print_err("Failed executing build script by runner project, result: %d.", r);
        exit(EXIT_FAILURE);
    }
}
}
