#include "run_deps_script.h"

#include <nowide/cstdio.hpp>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>

#include "clone.h"
#include "cmakex_utils.h"
#include "exec_process.h"
#include "filesystem.h"
#include "git.h"
#include "installdb.h"
#include "misc_utils.h"
#include "out_err_messages.h"
#include "print.h"
#include "run_add_pkgs.h"

namespace cmakex {

namespace fs = filesystem;

const char* k_build_script_add_pkg_out_filename = "add_pkg_out.txt";
const char* k_build_script_cmakex_out_filename = "cmakex_out.txt";
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

           "function(add_pkg NAME)\n"
           "  # test list compatibility\n"
           "  set(s ${NAME})\n"
           "  list(LENGTH s l)\n"
           "  if (NOT l EQUAL 1)\n"
           "    message(FATAL_ERROR \"\\\"${NAME}\\\" is an invalid name for a package\")\n"
           "  endif()\n"
           //"  message(STATUS \"file(APPEND \\\"${__CMAKEX_ADD_PKG_OUT}\\\" "
           //"\\\"${NAME};${ARGN}\\\\n\\\")\")\n"
           "  set(line \"${NAME}\")\n"
           "  foreach(x IN LISTS ARGN)\n"
           "    set(line \"${line}\\t${x}\")\n"
           "  endforeach()\n"
           "  file(APPEND \"${__CMAKEX_ADD_PKG_OUT}\" \"${line}\\n\")\n"
           "endfunction()\n\n"

           "# include build script within a function to protect local variables\n"
           "function(include_build_script path)\n"
           "  if(NOT IS_ABSOLUTE \"${path}\")\n"
           "    set(path \"${CMAKE_CURRENT_LIST_DIR}/${path}\")\n"
           "  endif()\n"
           "  if(NOT EXISTS \"${path}\")\n"
           "    message(FATAL_ERROR \"Build script not found: \\\"${path}\\\".\")\n"
           "  endif()\n"
           "  include(\"${path}\")\n"
           "endfunction()\n\n"

           "if(DEFINED command)\n"
           "  message(STATUS \"Build script executor command: ${command}\")\n"
           "  list(GET command 0 verb)\n\n"
           "  if(verb STREQUAL \"run\")\n"
           "    list(LENGTH command l)\n"
           "    if(NOT l EQUAL 3)\n"
           "      message(FATAL_ERROR \"Internal error, invalid command\")\n"
           "    endif()\n"
           "    list(GET command 1 path)\n"
           "    list(GET command 2 out)\n"
           "    if(NOT EXISTS \"${out}\" OR IS_DIRECTORY \"${out}\")\n"
           "      message(FATAL_ERROR \"Internal error, the output file "
           "\\\"${out}\\\" is not an existing file.\")\n"
           "    endif()\n"
           "    set(__CMAKEX_ADD_PKG_OUT \"${out}\")\n"
           "    include_build_script(\"${path}\")\n"
           "  endif()\n"
           "endif()\n\n";
}
string build_script_executor_cmakelists_checksum(const std::string& x)
{
    auto hs = std::to_string(std::hash<std::string>{}(x));
    return stringf("# script hash: %s", hs.c_str());
}

void fail_if_current_clone_has_different_commit(string req_git_tag,
                                                string_par clone_dir,
                                                string_par cloned_sha,
                                                string_par git_url)
{
    // if HEAD/branch is requested need to check ls-remote
    // if tag/SHA requested check local clone

    if (req_git_tag.empty())
        req_git_tag = "HEAD";

    string msg = stringf(
        "Because of the '--strict' option the directory \"%s\" should be "
        "reset to the remote's '%s' commit in order to build it. Reset manually or remove the "
        "directory.",
        clone_dir.c_str(), req_git_tag.c_str());

    if (cloned_sha == k_sha_uncommitted)  // no need to check further
        throwf("%s", msg.c_str());
    // find out if we have the exact same commit cloned out as req_git_tag
    // req_git_tag can be empty=branch/tag/sha
    if (cloned_sha != req_git_tag) {  // req_git_tag was an SHA
        // check remote
        auto lsr = git_ls_remote(git_url, req_git_tag);
        if (get<0>(lsr) == 0) {
            if (cloned_sha != get<1>(lsr))
                throwf("%s", msg.c_str());
        } else {
            throwf(
                "Because of the '--strict' option the requested ref '%s' "
                "needs to be resolved by the remote \"%s\" but 'git "
                "ls-remote' failed (%d)",
                req_git_tag.c_str(), git_url.c_str(), get<0>(lsr));
        }
    }
}
void run_deps_script(string binary_dir,
                     string deps_script_file,
                     const vector<string>& config_args,
                     const vector<string>& configs,
                     bool strict_commits)
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
    CHECK(!binary_dir.empty());

    if (fs::path(deps_script_file).is_relative())
        deps_script_file = fs::current_path().string() + "/" + deps_script_file;

    log_info("Processing dependency script \"%s\"", deps_script_file.c_str());

    if (fs::path(binary_dir).is_relative())
        binary_dir = fs::current_path().string() + "/" + binary_dir;

    const cmakex_config_t cfg(binary_dir);

    const string build_script_executor_binary_dir =
        cfg.cmakex_executor_dir + "/" + k_default_binary_dirname;

    const string build_script_add_pkg_out_file =
        cfg.cmakex_tmp_dir + "/" + k_build_script_add_pkg_out_filename;

    const string build_script_cmakex_out_file =
        cfg.cmakex_tmp_dir + "/" + k_build_script_cmakex_out_filename;

    for (auto d : {cfg.cmakex_executor_dir, cfg.cmakex_tmp_dir}) {
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
    const string cmakelists_text = build_script_executor_cmakelists();
    const string cmakelists_text_hash = build_script_executor_cmakelists_checksum(cmakelists_text);

    // force write if not exists or first hash line differs
    string cmakelists_path = cfg.cmakex_executor_dir + "/CMakeLists.txt";
    bool cmakelists_exists = fs::exists(cmakelists_path);
    if (cmakelists_exists) {
        cmakelists_exists = false;  // if anything goes wrong, pretend it doesn't exist
        do {
            FILE* f = nowide::fopen(cmakelists_path.c_str(), "r");
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
        auto f = must_fopen(cmakelists_path.c_str(), "w");
        must_fprintf(f, "%s\n%s\n", cmakelists_text_hash.c_str(), cmakelists_text.c_str());
    }

    vector<string> args;
    args.emplace_back(string("-H") + cfg.cmakex_executor_dir);
    args.emplace_back(string("-B") + build_script_executor_binary_dir);

    args.insert(args.end(), BEGINEND(config_args));
    args.emplace_back(string("-U") + k_executor_project_command_cache_var);
    log_info("Configuring build script executor project.");
    log_exec("cmake", args);
    OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
    int r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
    auto oem = oeb.move_result();

    save_log_from_oem(oem, cfg.cmakex_log_dir,
                      string(k_build_script_executor_log_name) + "-configure" + k_log_extension);

    if (r != EXIT_SUCCESS)
        throwf("Failed configuring build script executor project, result: %d.", r);

    args.clear();
    args.emplace_back(build_script_executor_binary_dir);

    // create empty add_pkg out file
    {
        auto f = must_fopen(build_script_add_pkg_out_file.c_str(), "w");
    }
    args.emplace_back(string("-D") + k_executor_project_command_cache_var + "=run;" +
                      deps_script_file + ";" + build_script_add_pkg_out_file);

    log_info("Executing dependencies script by executor project.");
    log_exec("cmake", args);
    OutErrMessagesBuilder oeb2(pipe_capture, pipe_capture);
    r = exec_process("cmake", args, oeb2.stdout_callback(), oeb2.stderr_callback());
    auto oem2 = oeb2.move_result();
    save_log_from_oem(oem2, cfg.cmakex_log_dir,
                      string(k_build_script_executor_log_name) + "-run" + k_log_extension);
    if (r != EXIT_SUCCESS)
        throwf("Failed executing build script by executor project, result: %d.", r);

    // read the add_pkg_out
    auto addpkgs_lines = must_read_file_as_lines(build_script_add_pkg_out_file);
    // for each pkg:
    for (auto& addpkg_line : addpkgs_lines) {
        auto pkg_request = pkg_request_from_args(split(addpkg_line, '\t'));
        // todo: get data from optional package registry here

        // fill configs from global par if not specified
        if (pkg_request.configs.empty())
            pkg_request.configs = configs;

        tuple<pkg_clone_dir_status_t, string> clone_status;
        string cloned_sha;
        bool cloned;

        auto update_clone_status_vars = [&clone_status, &cloned_sha, &cloned, &pkg_request,
                                         &binary_dir]() {
            // determine cloned status
            clone_status = pkg_clone_dir_status(binary_dir, pkg_request.name);
            cloned = false;
            switch (get<0>(clone_status)) {
                case pkg_clone_dir_doesnt_exist:
                case pkg_clone_dir_empty:
                    break;
                case pkg_clone_dir_git:
                    cloned_sha = get<1>(clone_status);
                    cloned = true;
                    break;
                case pkg_clone_dir_git_local_changes:
                case pkg_clone_dir_nonempty_nongit:
                    cloned_sha = k_sha_uncommitted;
                    cloned = true;
                    break;
                default:
                    CHECK(false);
            }
        };

        update_clone_status_vars();

        auto clone_this = []() {};
        auto install_this = []() {};
        auto uninstall_this = []() {};

        // determine installed status
        InstallDB installdb(binary_dir);
        auto installed_result = installdb.evaluate_pkg_request(pkg_request);
        string clone_dir = cfg.cmakex_deps_clone_prefix + "/" + pkg_request.name;
        switch (installed_result.status) {
            case InstallDB::pkg_request_not_installed:
                if (cloned)
                    clone_this();
                if (cloned && strict_commits)
                    fail_if_current_clone_has_different_commit(
                        pkg_request.clone_pars.git_tag,
                        cfg.cmakex_deps_clone_prefix + "/" + pkg_request.name, cloned_sha,
                        pkg_request.clone_pars.git_url);
                // cloning it now, SHA will be as requested
                install_this();
                break;
            case InstallDB::pkg_request_missing_configs: {
                bool was_cloned = cloned;
                if (!cloned) {
                    clone_this();
                    update_clone_status_vars();
                }
                if (was_cloned && strict_commits)
                    fail_if_current_clone_has_different_commit(pkg_request.clone_pars.git_tag,
                                                               clone_dir, cloned_sha,
                                                               pkg_request.clone_pars.git_url);
                if (cloned_sha == k_sha_uncommitted ||
                    cloned_sha != installed_result.pkg_desc.git_sha)
                    uninstall_this();
                install_this();
            } break;
            case InstallDB::pkg_request_satisfied:
                if (strict_commits) {
                    string req_git_tag = pkg_request.clone_pars.git_url;
                    if (req_git_tag.empty())
                        req_git_tag = "HEAD";
                    string remote_sha;
                    try {
                        remote_sha = must_git_resolve_ref_on_remote(
                            req_git_tag, pkg_request.clone_pars.git_url, true);
                    } catch (const exception& e) {
                        throwf(
                            "Because of the '--strict' option the directory \"%s\" should be reset "
                            "to the remote's '%s' commit in order to build it. On resolving the "
                            "commit 'git ls-remote' "
                            "failed for \"%s\", reason: %s. Reset manually or remove the "
                            "directory.",
                            clone_dir.c_str(), req_git_tag.c_str(),
                            pkg_request.clone_pars.git_url.c_str(), e.what());
                    }
                    if (remote_sha == installed_result.pkg_desc.git_sha)
                        break;
                    bool sha_like = remote_sha == req_git_tag;

                    if (cloned) {
                        string resolved_remote_sha;
                        if (sha_like) {
                            // if remote_sha is a shortened sha it can still be equal to
                            // installed sha
                            // check it in the clone
                            resolved_remote_sha = git_rev_parse(remote_sha, clone_dir);
                            if (resolved_remote_sha.empty())
                                throwf(
                                    "Because of the '--strict' option the requested commit "
                                    "'%s' "
                                    "has to be resolved but neither 'git ls-remote' on \"%s\" "
                                    "nor "
                                    "'git rev-parse' in \"%s\" was able to do it. Reset the "
                                    "repository manually or remove the directory.",
                                    req_git_tag.c_str(), pkg_request.clone_pars.git_url.c_str(),
                                    clone_dir.c_str());
                            if (resolved_remote_sha == installed_result.pkg_desc.git_sha)
                                break;
                        } else
                            resolved_remote_sha = remote_sha;

                        // we may still be able to build it
                        if (resolved_remote_sha != cloned_sha)
                            throwf(
                                "Because of the '--strict' option the directory \"%s\" "
                                "should be "
                                "reset to the remote's '%s' commit in order to build "
                                "it. Reset manually or remove the "
                                "directory.",
                                clone_dir.c_str(), req_git_tag.c_str());
                        install_this();
                    } else {
                        // not cloned
                        clone_this();
                        update_clone_status_vars();
                        if (cloned_sha != installed_result.pkg_desc.git_sha)
                            install_this();
                    }
                }
                break;
            case InstallDB::pkg_request_not_compatible:
                if (cloned) {
                    if (strict_commits)
                        fail_if_current_clone_has_different_commit(pkg_request.clone_pars.git_url,
                                                                   clone_dir, cloned_sha,
                                                                   pkg_request.clone_pars.git_url);
                } else
                    clone_this();
                uninstall_this();
                install_this();
                break;
            default:
                CHECK(false);
        }
    }
}
}
