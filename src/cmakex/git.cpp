#include "git.h"

#include <mutex>

#include <nowide/cstdio.hpp>

#include <adasworks/sx/mutex.h>

#include "filesystem.h"
#include "misc_utils.h"
#include "print.h"

namespace cmakex {
using adasworks::sx::atomic_flag_mutex;
using lock_guard = std::lock_guard<atomic_flag_mutex>;
namespace fs = filesystem;

namespace {
atomic_flag_mutex s_git_executable_mutex;
string s_git_executable;
}

string find_git_with_cmake()
{
    string resolved_path;

    auto tmpdir = fs::temp_directory_path().string();
    string script_path;
    const char* filename_base = "cmakex_findgit-";
    for (int i = 0; i < 1000; ++i) {
        string p = stringf("%s/%s%d.cmake", tmpdir.c_str(), filename_base, i);
        if (!fs::exists(p)) {
            script_path = p;
            break;
        }
    }
    if (script_path.empty()) {
        throwf("Can't create temporary file in \"%s\", please remove all the \"%s*\" files.",
               tmpdir.c_str(), filename_base);
    }

    try {
        // save findgit script to temporary file
        {
            auto f = must_fopen(script_path, "w");
            must_fprintf(f, "find_package(Git QUIET)\nmessage(\"${GIT_EXECUTABLE}\")\n");
        }

        // call it with cmake -P
        OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
        exec_process("cmake", {"-P", script_path.c_str()}, oeb.stdout_callback(),
                     oeb.stderr_callback());
        auto oem = oeb.move_result();
        for (int i = 0; i < oem.size(); ++i) {
            auto msg = oem.at(i);
            if (msg.source == out_err_message_base_t::source_stderr) {
                resolved_path = strip_trailing_whitespace(msg.text);
                break;
            }
        }

        if (resolved_path.empty())
            throwf("Script with 'find_package(Git)' returned nothing.");
        if (!fs::exists(resolved_path))
            throwf("Result of find_package(Git) does not exist: \"%s\"", resolved_path.c_str());
    } catch (...) {
        fs::remove(script_path);
        throw;
    }
    fs::remove(script_path);
    return resolved_path;
}

string find_git_or_return_git()
{
    try {
        string result = find_git_with_cmake();
        log_info("Using git: %s.", result.c_str());
        return result;
    } catch (exception& e) {
        log_warn("Can't find git executable, using simply 'git'. Reason: %s", e.what());
        return "git";
    }
}

int exec_git(const vector<string>& args,
             string_par working_directory,
             exec_process_output_callback_t stdout_callback,
             exec_process_output_callback_t stderr_callback)
{
    string git_executable;
    {
        lock_guard lock(s_git_executable_mutex);
        git_executable = s_git_executable;
    }
    if (git_executable.empty()) {
        git_executable = find_git_or_return_git();
        if (git_executable.empty())
            throwf("Can't find git executable");
        else {
            lock_guard lock(s_git_executable_mutex);
            if (s_git_executable.empty())
                s_git_executable = git_executable;
            else
                git_executable = s_git_executable;
        }
    }

    // we have git here

    log_exec(git_executable, args, working_directory);

    return exec_process(git_executable, args, working_directory, stdout_callback, stderr_callback);
}

string first_line(const OutErrMessages& oem, out_err_message_base_t::source_t source)
{
    string s;
    for (int i = 0; i < oem.size(); ++i) {
        auto msg = oem.at(i);
        if (msg.source == source) {
            s = msg.text;
            break;
        }
    }
    return s;
}

tuple<int, string> git_ls_remote(string_par url, string_par ref)
{
    vector<string> args = {"ls-remote", "--exit-code", url.c_str(), ref.c_str()};
    OutErrMessagesBuilder oeb(pipe_capture, pipe_echo);
    int r = exec_git(args, oeb.stdout_callback(), nullptr);
    auto oem = oeb.move_result();
    auto s = first_line(oem, out_err_message_base_t::source_stdout);
    for (int i = 0; i < s.size(); ++i) {
        if (iswspace(s[i])) {
            s.resize(i);
            break;
        }
    }
    return {r, s};
}

string git_rev_parse_head(string_par dir)
{
    vector<string> args = {"rev-parse", "HEAD"};
    OutErrMessagesBuilder oeb(pipe_capture, pipe_echo);
    int r = exec_git(args, dir, oeb.stdout_callback(), nullptr);
    auto oem = oeb.move_result();
    if (r)
        return {};
    auto s = first_line(oem, out_err_message_base_t::source_stdout);
    return strip_trailing_whitespace(s);
}

void git_clone(vector<string> args)
{
    args.insert(args.begin(), "clone");
    int r = exec_git(args);
    if (r)
        throwf("git-clone failed with error code %d.", r);
}
int git_checkout(vector<string> args, string_par dir)
{
    args.insert(args.begin(), "checkout");
    return exec_git(args, dir);
}
}
