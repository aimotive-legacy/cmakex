#include "git.h"

#include <mutex>

#include <nowide/cstdio.hpp>

#include <adasworks/sx/check.h>
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
        throwf("Can't create temporary file in %s, please remove all the \"%s*\" files.",
               path_for_log(tmpdir).c_str(), filename_base);
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
            throwf("Result of find_package(Git) does not exist: %s",
                   path_for_log(resolved_path).c_str());
    } catch (...) {
        fs::remove(script_path);
        throw;
    }
    fs::remove(script_path);
    return resolved_path;
}

void test_git(string_par git_command)
{
    OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);
    int r = exec_process(git_command, vector<string>{{"--version"}}, oeb.stdout_callback(),
                         oeb.stderr_callback());
    auto oem = oeb.move_result();

    if (r) {
        auto p = getenv("PATH");
        throwf("Can't find git. Error code: %d, PATH: %s", r, p ? p : "<null>");
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

string find_git_or_return_git()
{
    string result;
    try {
        result = find_git_with_cmake();
        log_info("Using git: %s.", result.c_str());
    } catch (exception& e) {
        log_warn("Can't find git executable, using simply 'git'. Reason: %s", e.what());
        result = "git";
    }
    test_git(result);
    return result;
}

int exec_git(const vector<string>& args,
             string_par working_directory,
             exec_process_output_callback_t stdout_callback,
             exec_process_output_callback_t stderr_callback,
             log_git_command_t quiet_mode)
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

    if (quiet_mode == log_git_command_always)
        log_exec("git", args, working_directory);
    if (quiet_mode != log_git_command_never)
        CHECK(stderr_callback == nullptr);

    OutErrMessagesBuilder oeb(pipe_capture, pipe_capture);

    auto result = exec_process(
        git_executable, args, working_directory,
        quiet_mode != log_git_command_always && stdout_callback == nullptr ? oeb.stdout_callback()
                                                                           : stdout_callback,
        quiet_mode != log_git_command_always && stderr_callback == nullptr ? oeb.stderr_callback()
                                                                           : stderr_callback);

    if (quiet_mode == log_git_command_on_error && result)
        log_exec("git", args, working_directory);

    if (quiet_mode == log_git_command_always || (quiet_mode == log_git_command_never && result)) {
        OutErrMessages oem(oeb.move_result());
        for (int i = 0; i < oem.size(); ++i) {
            auto m = oem.at(i);
            fprintf(m.source == out_err_message_base_t::source_stdout ? stdout : stderr, "%s\n",
                    m.text.c_str());
        }
    }

    return result;
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
    int r = exec_git(args, oeb.stdout_callback(), nullptr, log_git_command_never);
    auto oem = oeb.move_result();
    auto s = first_line(oem, out_err_message_base_t::source_stdout);
    for (int i = 0; i < s.size(); ++i) {
        if (iswspace(s[i])) {
            s.resize(i);
            break;
        }
    }
    return make_tuple(r, move(s));
}

string git_rev_parse(string_par ref, string_par dir)
{
    vector<string> args = {"rev-parse", ref.c_str()};
    OutErrMessagesBuilder oeb(pipe_capture, pipe_echo);
    if (exec_git(args, dir, oeb.stdout_callback(), nullptr, log_git_command_on_error))
        return {};
    auto oem = oeb.move_result();
    auto s = first_line(oem, out_err_message_base_t::source_stdout);
    return strip_trailing_whitespace(s);
}

void git_clone(vector<string> args)
{
    args.insert(args.begin(), "clone");
    int r = exec_git(args, nullptr, nullptr, log_git_command_always);
    if (r)
        throwf("git-clone failed with error code %d.", r);
}
int git_checkout(vector<string> args, string_par dir)
{
    args.insert(args.begin(), "checkout");
    return exec_git(args, dir, nullptr, nullptr, log_git_command_always);
}
string try_find_unique_ref_by_sha_with_ls_remote(string_par git_url, string_par sha)
{
    OutErrMessagesBuilder oeb(pipe_capture, pipe_echo);
    int r = exec_git({"ls-remote", git_url.c_str()}, oeb.stdout_callback(), nullptr,
                     log_git_command_never);
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
        return make_tuple(resolve_ref_success, move(sha));
    if (r == 2) {
        if (sha_like(ref))
            return make_tuple(resolve_ref_sha_like, string{});
        else
            return make_tuple(resolve_ref_not_found, string{});
    }
    return make_tuple(resolve_ref_error, string{});
}

string must_git_resolve_ref_on_remote(string_par git_url, string_par ref, bool allow_sha)
{
    auto r = git_resolve_ref_on_remote(git_url, ref);
    switch (get<0>(r)) {
        case resolve_ref_error:
            throwf("'git ls-remote' failed.");
        case resolve_ref_success:
            return get<1>(r);
        case resolve_ref_not_found:
            throwf("Ref '%s' not found with 'git ls-remote'", ref.c_str());
        case resolve_ref_sha_like:
            if (allow_sha)
                return ref.str();
            else
                throwf("Ref '%s' not found with 'git ls-remote'", ref.c_str());
        default:
            CHECK(false);
    }
    // never here
    return {};
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

bool git_status_result_t::clean_or_untracked_only() const
{
    for (auto& l : lines) {
        if (l[0] != '?' || l[1] != '?')
            return false;
    }
    return true;
}

git_status_result_t git_status(string_par dir, bool branch_tracking)
{
    OutErrMessagesBuilder oeb(pipe_capture, pipe_echo);
    vector<string> args = {"status", "-s", "--porcelain"};
    if (branch_tracking)
        args.push_back("-b");

    int r = exec_git(args, dir, oeb.stdout_callback(), nullptr, log_git_command_on_error);
    if (r)
        throw runtime_error(
            stringf("git status failed (%d) for directory %s", r, path_for_log(dir).c_str()));
    auto oem = oeb.move_result();
    git_status_result_t result;
    for (int i = 0; i < oem.size(); ++i) {
        auto msg = oem.at(i);
        if (msg.source != out_err_message_base_t::source_stdout)
            continue;
        auto lines = split_at_newlines(msg.text);
        for (auto& l : lines) {
            if (l.size() >= 4)
                result.lines.emplace_back(move(l));
        }
    }
    return result;
}
}
