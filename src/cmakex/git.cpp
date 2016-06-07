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

string git_rev_parse(string_par ref, string_par dir)
{
    vector<string> args = {"rev-parse", ref.c_str()};
    OutErrMessagesBuilder oeb(pipe_capture, pipe_echo);
    if (exec_git(args, dir, oeb.stdout_callback(), nullptr))
        return {};
    auto oem = oeb.move_result();
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
string try_resolve_sha_to_tag(string_par git_url, string_par sha)
{
    OutErrMessagesBuilder oeb(pipe_capture, pipe_echo);
    int r = exec_git({"ls-remote", git_url.c_str()});
    if (r)
        return {};
    auto oem = oeb.move_result();
    vector<string> results;
    for (int i = 0; i < oem.size(); ++i) {
        auto msg = oem.at(i);
        if (msg.source != out_err_message_base_t::source_stdout || !istarts_with(msg.text, sha))
            continue;
        int j = 0;
        int x0 = -1, x1 = -1;
        while (j < msg.text.size()) {
            if (iswspace(msg.text[j])) {
                x0 = j + 1;
                break;
            }
        }
        if (x0 < 0)
            throwf("Invalid line in the output of git-ls-remote, no whitespace: %s",
                   msg.text.c_str());
        j = x0;
        x1 = msg.text.size();
        while (j < msg.text.size()) {
            if (iswspace(msg.text[j])) {
                x1 = j;
                break;
            }
        }
        if (x1 == x0)
            throwf("Invalid line in the output of git-ls-remote, no word after whitespace: %s",
                   msg.text.c_str());
        results.emplace_back(msg.text.substr(x0, x1 - x0));
    }
    if (results.empty())
        return {};
    if (results.size() > 1)
        throwf("git-ls-remote: requested SHA is ambiguous: '%s'", sha.c_str());
    return results.front();
}

tuple<resolve_ref_status_t, string> git_resolve_ref_on_remote(string_par git_url, string_par ref)
{
    int r;
    string sha;
    tie(r, sha) = git_ls_remote(git_url, ref);
    if (r == 0)
        return {resolve_ref_success, sha};
    if (r == 2) {
        if (sha_like(ref))
            return {resolve_ref_sha_like, string{}};
        else
            return {resolve_ref_not_found, string{}};
    }
    return {resolve_ref_error, string{}};
}

bool sha_like(string_par x)
{
    if (x.size() < 4 || x.size() > 40)
        return false;
    for (const char* c = x.c_str(); *c; ++c)
        if (!isxdigit(*c))
            return false;
    return true;
}
}
