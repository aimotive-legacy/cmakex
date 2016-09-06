#include "helper_cmake_project.h"

#include <nowide/cstdio.hpp>

#include "cmakex-types.h"
#include "cmakex_utils.h"
#include "filesystem.h"
#include "misc_utils.h"
#include "out_err_messages.h"
#include "print.h"
#include "resource.h"

namespace cmakex {

namespace fs = filesystem;

const char* k_build_script_add_pkg_out_filename = "add_pkg_out.txt";
const char* k_build_script_cmakex_out_filename = "cmakex_out.txt";
const char* k_default_binary_dirname = "b";
const char* k_executor_project_command_cache_var = "__CMAKEX_EXECUTOR_PROJECT_COMMAND";
const char* k_build_script_executor_log_name = "deps_script_wrapper";
static const char* const cmakex_version_mmp = STRINGIZE(CMAKEX_VERSION_MMP);

string deps_script_wrapper_cmakelists()
{
    return stringf(
               "cmake_minimum_required(VERSION ${CMAKE_VERSION})\n\n"
               "if(DEFINED %s)\n"
               "    set(command \"${%s}\")\n"
               "    unset(%s CACHE)\n"
               "endif()\n\n",
               k_executor_project_command_cache_var, k_executor_project_command_cache_var,
               k_executor_project_command_cache_var) +
           k_deps_script_wrapper_cmakelists_body;
}
string deps_script_wrapper_cmakelists_checksum(const std::string& x)
{
    auto hs = std::to_string(std::hash<std::string>{}(x));
    return stringf("# script hash: %s", hs.c_str());
}

HelperCmakeProject::HelperCmakeProject(string_par binary_dir)
    : binary_dir(fs::absolute(binary_dir.c_str()).string()),
      cfg(binary_dir),
      build_script_executor_binary_dir(cfg.cmakex_executor_dir() + "/" + k_default_binary_dirname),
      build_script_add_pkg_out_file(cfg.cmakex_tmp_dir() + "/" +
                                    k_build_script_add_pkg_out_filename),
      build_script_cmakex_out_file(cfg.cmakex_tmp_dir() + "/" + k_build_script_cmakex_out_filename)
{
}

void test_cmake()
{
    OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
    int r = exec_process("cmake", vector<string>{{"--version"}}, oeb.stdout_callback(),
                         oeb.stderr_callback());
    auto oem = oeb.move_result();

    if (r) {
        auto p = getenv("PATH");
        throwf("Can't find cmake executable on the path. Error code: %d, PATH: %s", r,
               p ? p : "<null>");
    } else {
        if (oem.size() >= 1) {
            auto msg = oem.at(0);
            auto pos = std::min(msg.text.find('\n'), msg.text.find('\r'));
            if (pos != string::npos)
                msg.text = msg.text.substr(0, pos);
            printf("%s\n", msg.text.c_str());
        }
    }
}
void HelperCmakeProject::configure(const vector<string>& command_line_cmake_args)
{
    test_cmake();
    for (auto d : {cfg.cmakex_executor_dir(), cfg.cmakex_tmp_dir()}) {
        if (!fs::is_directory(d)) {
            string msg;
            try {
                fs::create_directories(d);
            } catch (const exception& e) {
                msg = e.what();
            } catch (...) {
                msg = "unknown exception";
            }
            if (!msg.empty())
                throwf("Can't create directory \"%s\", reason: %s.", d.c_str(), msg.c_str());
        }
    }

    // create the text of CMakelists.txt
    const string cmakelists_text = deps_script_wrapper_cmakelists();
    const string cmakelists_text_hash = deps_script_wrapper_cmakelists_checksum(cmakelists_text);

    // force write if not exists or first hash line differs
    string cmakelists_path = cfg.cmakex_executor_dir() + "/CMakeLists.txt";
    bool cmakelists_exists = fs::exists(cmakelists_path);
    if (cmakelists_exists) {
        cmakelists_exists = false;  // if anything goes wrong, pretend it doesn't exist
        do {
            FILE* f = nowide::fopen(cmakelists_path.c_str(), "r");
            if (!f)
                break;
            const int c_bufsize = 128;
            char buf[c_bufsize];
            buf[0] = 0;
            auto r = fgets(buf, c_bufsize, f);
            (void)r;  // does not count, the hash will decide in the next line
            cmakelists_exists = strncmp(buf, cmakelists_text_hash.c_str(), c_bufsize) == 0;
            fclose(f);
        } while (false);
    }
    if (!cmakelists_exists) {
        auto f = must_fopen(cmakelists_path.c_str(), "w");
        must_fprintf(f, "%s\n%s\n", cmakelists_text_hash.c_str(), cmakelists_text.c_str());
    }

    vector<string> args;
    args.emplace_back(string("-H") + cfg.cmakex_executor_dir());
    args.emplace_back(string("-B") + build_script_executor_binary_dir);

    fs::create_directories(build_script_executor_binary_dir);

    bool initial_config =
        !fs::is_regular_file(build_script_executor_binary_dir + "/CMakeCache.txt");

    if (initial_config)
        remove_cmake_cache_tracker(build_script_executor_binary_dir);

    //    CMakeCacheTracker cvt(build_script_executor_binary_dir);

    auto cct = load_cmake_cache_tracker(build_script_executor_binary_dir);
    cct.add_pending(command_line_cmake_args);
    save_cmake_cache_tracker(build_script_executor_binary_dir, cct);

    append_inplace(args, cct.pending_cmake_args);
    args.emplace_back(string("-U") + k_executor_project_command_cache_var);
    args.emplace_back(string{"-DCMAKEX_VERSION="} + cmakex_version_mmp);
    auto cl_config = string_exec("cmake", args);
    OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
    int r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
    auto oem = oeb.move_result();

    if (r)
        printf("%s\n", cl_config.c_str());
    save_log_from_oem(cl_config, r, oem, cfg.cmakex_log_dir(),
                      string(k_build_script_executor_log_name) + "-configure" + k_log_extension);

    string cmake_cache_path = build_script_executor_binary_dir + "/CMakeCache.txt";
    if (r != EXIT_SUCCESS) {
        if (initial_config && fs::is_regular_file(cmake_cache_path))
            fs::remove(cmake_cache_path);
        throwf("Failed configuring dependency script wrapper project, result: %d.", r);
    }

    cct.confirm_pending();
    save_cmake_cache_tracker(build_script_executor_binary_dir, cct);

    cmake_cache = read_cmake_cache(cmake_cache_path);
}

vector<string> HelperCmakeProject::run_deps_script(string_par deps_script_file,
                                                   bool clear_downloaded_include_files)
{
    vector<string> args;
    args.emplace_back(build_script_executor_binary_dir);

    // create empty add_pkg out file
    {
        auto f = must_fopen(build_script_add_pkg_out_file.c_str(), "w");
    }
    args.emplace_back(string("-D") + k_executor_project_command_cache_var + "=run;" +
                      deps_script_file.c_str() + ";" + build_script_add_pkg_out_file);
    if (clear_downloaded_include_files)
        args.emplace_back("-D__CMAKEX_INCL_CLEAR_DOWNLOAD_DIR=1");

    auto cl_deps = string_exec("cmake", args);
    OutErrMessagesBuilder oeb2(pipe_capture, pipe_capture);
    int r = exec_process("cmake", args, oeb2.stdout_callback(), oeb2.stderr_callback());
    auto oem2 = oeb2.move_result();
    if (r)
        printf("%s\n", cl_deps.c_str());
    save_log_from_oem(cl_deps, r, oem2, cfg.cmakex_log_dir(),
                      string(k_build_script_executor_log_name) + "-run" + k_log_extension);
    if (r != EXIT_SUCCESS)
        throwf("Failed executing dependency script wrapper, result: %d.", r);

    // read the add_pkg_out
    return must_read_file_as_lines(build_script_add_pkg_out_file);
}
}
