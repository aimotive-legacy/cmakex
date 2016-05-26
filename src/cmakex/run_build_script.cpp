#include "run_build_script.h"

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

#include "exec_process.h"
#include "filesystem.h"
#include "misc_util.h"
#include "out_err_messages.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;

const char* k_build_script_executor_subdir = "_cmakex/build_script_executor_project";
const char* k_log_subdir = "_cmakex/log";
const char* k_default_binary_dirname = "b";
const char* k_executor_project_command_cache_var = "__CMAKEX_EXECUTOR_PROJECT_COMMAND";
const char* k_build_script_executor_log_name = "build_script_executor";
const char* k_log_extension = ".log";

string build_script_executor_cmakelists()
{
    return stringf(
               "cmake_minimum_required(VERSION ${CMAKE_VERSION})\n\n"
               "if(DEFINED %s)\n"
               "    set(command \"${%s}\")\n"
               "    unset(%s CACHE)\n"
               "endif()\n\n",
               k_executor_project_command_cache_var, k_executor_project_command_cache_var,
               k_executor_project_command_cache_var) +

           "# include build script within a function to protect local variables\n"
           "function(include_build_script path)\n"
           "    if(NOT IS_ABSOLUTE \"${path}\")\n"
           "        set(path \"${CMAKE_CURRENT_LIST_DIR}/${path}\")\n"
           "    endif()\n"
           "    if(NOT EXISTS \"${path}\")\n"
           "        message(FATAL_ERROR \"Build script not found: \\\"${path}\\\".\")\n"
           "    endif()\n"
           "    include(\"${path}\")\n"
           "endfunction()\n\n"

           "if(DEFINED command)\n"
           "    message(STATUS \"Build script executor command: ${command}\")\n"
           "    list(GET command 0 verb)\n\n"
           "    if(verb STREQUAL \"run\")\n"
           "        list(GET command 1 path)\n"
           "        include_build_script(\"${path}\")\n"
           "    endif()\n"
           "endif()\n\n";
}
string build_script_executor_cmakelists_checksum(const std::string& x)
{
    auto hs = std::to_string(std::hash<std::string>{}(x));
    return stringf("# script hash: %s", hs.c_str());
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
    string build_script_executor_source_dir =
        pars.binary_dir + "/" + k_build_script_executor_subdir;
    string build_script_executor_binary_dir =
        build_script_executor_source_dir + "/" + k_default_binary_dirname;

    if (!fs::is_directory(build_script_executor_source_dir)) {
        string msg;
        try {
            fs::create_directories(build_script_executor_source_dir);
        } catch (const exception& e) {
            msg = e.what();

        } catch (...) {
            msg = "unknown exception";
        }
        if (!msg.empty()) {
            print_err("Can't create directory \"%s\", reason: %s.",
                      build_script_executor_source_dir.c_str(), msg.c_str());
            exit(EXIT_FAILURE);
        }
    }

    // create the text of CMakelists.txt
    const string cmakelists_text = build_script_executor_cmakelists();
    const string cmakelists_text_hash = build_script_executor_cmakelists_checksum(cmakelists_text);

    // force write if not exists or first hash line differs
    string cmakelists_path = build_script_executor_source_dir + "/CMakeLists.txt";
    bool cmakelists_exists = fs::exists(cmakelists_path);
    if (cmakelists_exists) {
        cmakelists_exists = false;  // if anything goes wrong, pretend it doesn't exist
        do {
            FILE* f = fopen(cmakelists_path.c_str(), "rt");
            if (!f)
                break;
            int c_bufsize = 128;
            char buf[c_bufsize];
            buf[0] = 0;
            fgets(buf, c_bufsize, f);
            cmakelists_exists = strncmp(buf, cmakelists_text_hash.c_str(), c_bufsize) == 0;
            fclose(f);
        } while (false);
    }
    if (!cmakelists_exists) {
        FILE* f = fopen(cmakelists_path.c_str(), "wt");
        if (!f) {
            print_err("Can't open \"%s\" for writing", cmakelists_path.c_str());
            exit(EXIT_FAILURE);
        }
        int r = fprintf(f, "%s\n%s\n", cmakelists_text_hash.c_str(), cmakelists_text.c_str());
        int was_errno = errno;
        fclose(f);
        if (r < 0) {
            print_err("Write error for \"%s\", reason: (%d) %s", cmakelists_path.c_str(), was_errno,
                      strerror(was_errno));
            exit(EXIT_FAILURE);
        }
    }

    vector<string> args;
    args.emplace_back(string("-H") + build_script_executor_source_dir);
    args.emplace_back(string("-B") + build_script_executor_binary_dir);

    args.insert(args.end(), BEGINEND(pars.config_args));
    args.emplace_back(string("-U") + k_executor_project_command_cache_var);
    print_out("Configuring build script executor project.");
    log_exec("cmake", args);
    OutErrMessagesBuilder oeb(false);
    int r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
    auto oem = oeb.move_result();

    string log_dir = pars.binary_dir + "/" + k_log_subdir;
    save_log_from_oem(oem, log_dir,
                      string(k_build_script_executor_log_name) + "-configure" + k_log_extension);

    if (r != EXIT_SUCCESS) {
        print_err("Failed configuring build script executor project, result: %d.", r);
        exit(EXIT_FAILURE);
    }

    args.clear();
    args.emplace_back(build_script_executor_binary_dir);
    string source_desc = pars.source_desc;
    if (fs::path(source_desc).is_relative())
        source_desc = fs::current_path().string() + "/" + source_desc;
    args.emplace_back(string("-D") + k_executor_project_command_cache_var + "=run;" + source_desc);

    print_out("Executing build script by executor project.");
    log_exec("cmake", args);
    OutErrMessagesBuilder oeb2(false);
    r = exec_process("cmake", args, oeb2.stdout_callback(), oeb2.stderr_callback());
    auto oem2 = oeb2.move_result();
    save_log_from_oem(oem2, log_dir,
                      string(k_build_script_executor_log_name) + "-run" + k_log_extension);
    if (r != EXIT_SUCCESS) {
        print_err("Failed executing build script by executor project, result: %d.", r);
        exit(EXIT_FAILURE);
    }
}
}
