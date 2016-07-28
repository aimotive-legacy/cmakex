#include "install_deps_phase_one.h"

#include <nowide/cstdio.hpp>

#include <adasworks/sx/algorithm.h>
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
const char* k_build_script_executor_log_name = "deps_script_wrapper";

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

           "# include deps script within a function to protect local variables\n"
           "function(include_deps_script path)\n"
           "  if(NOT IS_ABSOLUTE \"${path}\")\n"
           "    set(path \"${CMAKE_CURRENT_LIST_DIR}/${path}\")\n"
           "  endif()\n"
           "  if(NOT EXISTS \"${path}\")\n"
           "    message(FATAL_ERROR \"Dependency script not found: \\\"${path}\\\".\")\n"
           "  endif()\n"
           "  include(\"${path}\")\n"
           "endfunction()\n\n"

           "if(DEFINED command)\n"
           "  message(STATUS \"Dependency script wrapper command: ${command}\")\n"
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
           "    include_deps_script(\"${path}\")\n"
           "  endif()\n"
           "endif()\n\n";
}
string deps_script_wrapper_cmakelists_checksum(const std::string& x)
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

vector<string> install_deps_phase_one_deps_script(string_par binary_dir,
                                                  string_par deps_script_filename,
                                                  const vector<string>& global_cmake_args,
                                                  const vector<string>& configs,
                                                  deps_recursion_wsp_t& wsp,
                                                  const cmakex_cache_t& cmakex_cache);

vector<string> install_deps_phase_one_request_deps(string_par binary_dir,
                                                   vector<string> request_deps,
                                                   const vector<string>& global_cmake_args,
                                                   const vector<string>& configs,
                                                   deps_recursion_wsp_t& wsp,
                                                   const cmakex_cache_t& cmakex_cache)
{
    vector<string> pkgs_encountered;

    // for each pkg:
    for (auto& d : request_deps) {
        auto pkgs_encountered_below =
            run_deps_add_pkg({{d}}, binary_dir, global_cmake_args, configs, wsp, cmakex_cache);
        pkgs_encountered.insert(pkgs_encountered.end(), BEGINEND(pkgs_encountered_below));
    }

    return pkgs_encountered;
}

vector<string> install_deps_phase_one(string_par binary_dir,
                                      string_par source_dir,
                                      vector<string> request_deps,
                                      const vector<string>& global_cmake_args,
                                      const vector<string>& configs,
                                      deps_recursion_wsp_t& wsp,
                                      const cmakex_cache_t& cmakex_cache)
{
    CHECK(!binary_dir.empty());
    if (!source_dir.empty()) {
        string deps_script_file = fs::lexically_normal(fs::absolute(source_dir.str()).string() +
                                                       "/" + k_deps_script_filename);
        if (fs::is_regular_file(deps_script_file))
            return install_deps_phase_one_deps_script(
                binary_dir, deps_script_file, global_cmake_args, configs, wsp, cmakex_cache);
    }
    return install_deps_phase_one_request_deps(binary_dir, request_deps, global_cmake_args, configs,
                                               wsp, cmakex_cache);
}

vector<string> install_deps_phase_one_deps_script(string_par binary_dir_sp,
                                                  string_par deps_script_file,
                                                  const vector<string>& global_cmake_args,
                                                  const vector<string>& configs,
                                                  deps_recursion_wsp_t& wsp,
                                                  const cmakex_cache_t& cmakex_cache)
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

    log_info("Processing dependency script \"%s\"", deps_script_file.c_str());

    auto binary_dir = binary_dir_sp.str();

    if (fs::path(binary_dir).is_relative())
        binary_dir = fs::absolute(binary_dir.c_str()).string();

    const cmakex_config_t cfg(binary_dir);

    const string build_script_executor_binary_dir =
        cfg.cmakex_executor_dir() + "/" + k_default_binary_dirname;

    const string build_script_add_pkg_out_file =
        cfg.cmakex_tmp_dir() + "/" + k_build_script_add_pkg_out_filename;

    const string build_script_cmakex_out_file =
        cfg.cmakex_tmp_dir() + "/" + k_build_script_cmakex_out_filename;

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
    args.emplace_back(string("-H") + cfg.cmakex_executor_dir());
    args.emplace_back(string("-B") + build_script_executor_binary_dir);

    args.insert(args.end(), BEGINEND(config_args));
    args.emplace_back(string("-U") + k_executor_project_command_cache_var);
    log_info("Configuring dependency script wrapper project.");
    log_exec("cmake", args);
    OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
    int r = exec_process("cmake", args, oeb.stdout_callback(), oeb.stderr_callback());
    auto oem = oeb.move_result();

    save_log_from_oem("CMake-configure", r, oem, cfg.cmakex_log_dir(),
                      string(k_build_script_executor_log_name) + "-configure" + k_log_extension);

    if (r != EXIT_SUCCESS)
        throwf("Failed configuring dependency script wrapper project, result: %d.", r);

    write_cmakex_cache_if_dirty(cmakex_cache);

    args.clear();
    args.emplace_back(build_script_executor_binary_dir);

    // create empty add_pkg out file
    {
        auto f = must_fopen(build_script_add_pkg_out_file.c_str(), "w");
    }
    args.emplace_back(string("-D") + k_executor_project_command_cache_var + "=run;" +
                      deps_script_file.c_str() + ";" + build_script_add_pkg_out_file);

    log_info("Executing dependency script wrapper.");
    log_exec("cmake", args);
    OutErrMessagesBuilder oeb2(pipe_capture, pipe_capture);
    r = exec_process("cmake", args, oeb2.stdout_callback(), oeb2.stderr_callback());
    auto oem2 = oeb2.move_result();
    save_log_from_oem("Dependency script", r, oem2, cfg.cmakex_log_dir(),
                      string(k_build_script_executor_log_name) + "-run" + k_log_extension);
    if (r != EXIT_SUCCESS)
        throwf("Failed executing dependency script wrapper, result: %d.", r);

    // read the add_pkg_out
    auto addpkgs_lines = must_read_file_as_lines(build_script_add_pkg_out_file);

    vector<string> pkgs_encountered;

    // for each pkg:
    for (auto& addpkg_line : addpkgs_lines) {
        auto pkgs_encountered_below = run_deps_add_pkg(split(addpkg_line, '\t'), binary_dir,
                                                       config_args, configs, strict_commits, wsp);
        pkgs_encountered.insert(pkgs_encountered.end(), BEGINEND(pkgs_encountered_below));
    }

    return pkgs_encountered;
}

vector<string> run_deps_add_pkg(const vector<string>& args,
                                string_par binary_dir,
                                const vector<string>& config_args,
                                const vector<string>& configs,
                                bool strict_commits,
                                deps_recursion_wsp_t& wsp)
{
    const cmakex_config_t cfg(binary_dir);

    auto pkg_request = pkg_request_from_args(args);

    pkg_request.b.cmake_args.insert(pkg_request.b.cmake_args.begin(), BEGINEND(config_args));

    auto pkg_cache_cmake_args =
        cmakex_cache_load(cfg.pkg_binary_dir_common(pkg_request.name), pkg_request.name);
    pkg_request.b.cmake_args.insert(pkg_request.b.cmake_args.begin(),
                                    BEGINEND(pkg_cache_cmake_args));

    // make_canonical_args also merges -D and -U switches of the same variable in correct order
    pkg_request.b.cmake_args = make_canonical_cmake_args(pkg_request.b.cmake_args);

    if (std::find(BEGINEND(wsp.requester_stack), pkg_request.name) != wsp.requester_stack.end()) {
        string s;
        for (auto& x : wsp.requester_stack) {
            if (!s.empty())
                s += " -> ";
            s += x;
        }
        s += "- > ";
        s += pkg_request.name;
        throwf("Circular dependency: %s", s.c_str());
    }

    // if it's already installed we still need to process this:
    // - to enumerate all dependencies
    // - to check if only compatible installations are requested

    // todo: get data from optional package registry here

    // fill configs from global par if not specified
    if (pkg_request.b.configs.empty())
        pkg_request.b.configs = configs;

    // it's already processed
    // 1. we may add a new configs to the planned ones
    // 2. must check if other args are compatible
    const bool already_processed = wsp.pkg_map.count(pkg_request.name) > 0;
    if (already_processed) {
        auto& pd = wsp.pkg_map[pkg_request.name].planned_desc;
        // compare SOURCE_DIR
        auto& s1 = pd.b.source_dir;
        auto& s2 = pkg_request.b.source_dir;
        if (s1 != s2) {
            throwf(
                "Different SOURCE_DIR args for the same package. The package '%s' is being added "
                "for the second time. The first time the SOURCE_DIR was \"%s\" and not it's "
                "\"%s\".",
                pkg_request.name.c_str(), s1.c_str(), s2.c_str());
        }
        // compare CMAKE_ARGS
        auto v = incompatible_cmake_args(pd.b.cmake_args, pkg_request.b.cmake_args);
        if (!v.empty()) {
            throwf(
                "Different CMAKE_ARGS args for the same package. The package '%s' is being added "
                "for the second time but the following CMAKE_ARGS are incompatible with the first "
                "request: %s",
                pkg_request.name.c_str(), join(v, ", ").c_str());
        }
    }

    clone_helper_t clone_helper(binary_dir, pkg_request.name);
    auto& cloned = clone_helper.cloned;
    auto& cloned_sha = clone_helper.cloned_sha;

    auto clone_this = [&pkg_request, &wsp, &clone_helper] {
        clone_helper.clone(pkg_request.c, pkg_request.git_shallow);
        wsp.pkg_map[pkg_request.name].just_cloned = true;
    };

    string pkg_source_dir = cfg.pkg_clone_dir(pkg_request.name);
    if (!pkg_request.b.source_dir.empty())
        pkg_source_dir += "/" + pkg_request.b.source_dir;

    // determine installed status
    InstallDB installdb(binary_dir);
    auto installed_result =
        installdb.evaluate_pkg_request_build_pars(pkg_request.name, pkg_request.b);
    string clone_dir = cfg.pkg_clone_dir(pkg_request.name);
    switch (installed_result.status) {
        case pkg_request_not_installed:
            if (cloned && strict_commits)
                fail_if_current_clone_has_different_commit(pkg_request.c.git_tag, clone_dir,
                                                           cloned_sha, pkg_request.c.git_url);
            if (!cloned)
                clone_this();
            break;
        case pkg_request_missing_configs: {
            bool was_cloned = cloned;
            if (!cloned)
                clone_this();
            if (was_cloned && strict_commits)
                fail_if_current_clone_has_different_commit(pkg_request.c.git_tag, clone_dir,
                                                           cloned_sha, pkg_request.c.git_url);
        } break;
        case pkg_request_satisfied:
            if (strict_commits) {
                string req_git_tag = pkg_request.c.git_url;
                if (req_git_tag.empty())
                    req_git_tag = "HEAD";
                string remote_sha;
                try {
                    remote_sha =
                        must_git_resolve_ref_on_remote(req_git_tag, pkg_request.c.git_url, true);
                } catch (const exception& e) {
                    throwf(
                        "Because of the '--strict' option the directory \"%s\" should be reset "
                        "to the remote's '%s' commit in order to build it. On resolving the "
                        "commit 'git ls-remote' "
                        "failed for \"%s\", reason: %s. Reset manually or remove the "
                        "directory.",
                        clone_dir.c_str(), req_git_tag.c_str(), pkg_request.c.git_url.c_str(),
                        e.what());
                }
                if (remote_sha == installed_result.pkg_desc.c.git_tag)
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
                                req_git_tag.c_str(), pkg_request.c.git_url.c_str(),
                                clone_dir.c_str());
                        if (resolved_remote_sha == installed_result.pkg_desc.c.git_tag)
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
                } else {
                    // not cloned
                    clone_this();
                }
            }
            break;
        case pkg_request_not_compatible:
            if (cloned) {
                if (strict_commits)
                    fail_if_current_clone_has_different_commit(pkg_request.c.git_url, clone_dir,
                                                               cloned_sha, pkg_request.c.git_url);
            } else
                clone_this();
            break;
        default:
            CHECK(false);
    }

    string unresolved_git_tag = pkg_request.c.git_tag;

    if (cloned) {
        pkg_request.c.git_tag = cloned_sha;
    } else {
        CHECK(installed_result.status == pkg_request_satisfied);
        pkg_request.c.git_tag = installed_result.pkg_desc.c.git_tag;
    }

    if (already_processed) {
        // verify final, resolved commit SHAs
        auto& pm = wsp.pkg_map[pkg_request.name];
        string prev_git_tag = pm.planned_desc.c.git_tag;
        if (pkg_request.c.git_tag != prev_git_tag) {
            throwf(
                "Different GIT_TAG's: this package has already been added but with different "
                "GIT_TAG specification. Previous GIT_TAG was '%s', resolved as %s, current GIT_TAG "
                "is '%s', resolved as %s",
                pm.unresolved_git_tag.c_str(), prev_git_tag.c_str(), unresolved_git_tag.c_str(),
                pkg_request.c.git_tag.c_str());
        }
    }

    vector<string> pkgs_encountered;

    // process_deps
    {
        if (cloned) {
            wsp.requester_stack.emplace_back(pkg_request.name);

            pkgs_encountered =
                install_deps_phase_one(binary_dir, pkg_source_dir, pkg_request.depends, config_args,
                                       configs, strict_commits, wsp);

            CHECK(wsp.requester_stack.back() == pkg_request.name);
            wsp.requester_stack.pop_back();
        } else {
            // enumerate dependencies from description of installed package
            CHECK(installed_result.status == pkg_request_satisfied);
            pkgs_encountered = installed_result.pkg_desc.depends;
        }
    }

    std::sort(BEGINEND(pkgs_encountered));
    sx::unique_trunc(pkgs_encountered);

    auto& pm = wsp.pkg_map[pkg_request.name];
    auto& pd = pm.planned_desc;
    if (already_processed) {
        // extend configs if needed
        pd.b.configs.insert(pd.b.configs.end(), BEGINEND(pkg_request.b.configs));
        std::sort(BEGINEND(pd.b.configs));
        sx::unique_trunc(pd.b.configs);
        // extend depends if needed
        pd.depends.insert(pd.depends.end(), BEGINEND(pkgs_encountered));
        std::sort(BEGINEND(pd.depends));
        sx::unique_trunc(pd.depends);
    } else {
        wsp.build_order.push_back(pkg_request.name);
        pm.unresolved_git_tag = unresolved_git_tag;
        pd = pkg_request;
        pd.depends = pkgs_encountered;
    }
    pkgs_encountered.emplace_back(pkg_request.name);
    return pkgs_encountered;
}
}
