#ifndef GIT_23849382
#define GIT_23849382

#include "out_err_messages.h"
#include "using-decls.h"

namespace cmakex {

// special SHA value to indicate uncommited changes
// when comparing SHA's this should be evaluted as different from every SHA string even from itself
static const char* const k_sha_uncommitted = "<uncommitted>";

// find git with cmake's find_package(Git), on failure returns "git"
string find_git_or_return_git();

// the stdout/stderr of the git commands are either
// - forwarded to user functions
// - captured internally (if user function == nullptr)
enum log_git_command_t
{
    log_git_command_never,  // never log command line, stdout/err are either forwarded to user
                            // function or ignored
    log_git_command_on_error,  // log command line + stderr only on error (stderr cannot be
                               // forwarded to user function in this case). Stdout will be also
                               // logged if not captured by user function.
    log_git_command_always  // log command line + stderr always (stderr cannot be forwarded to user
                            // function in this case). Stdout will be also logged if not captured by
                            // user function.
};

int exec_git(const vector<string>& args,
             string_par working_directory,
             exec_process_output_callback_t stdout_callback,
             exec_process_output_callback_t stderr_callback,
             log_git_command_t quiet_mode);

inline int exec_git(const vector<string>& args,
                    exec_process_output_callback_t stdout_callback,
                    exec_process_output_callback_t stderr_callback,
                    log_git_command_t quiet_mode)
{
    return exec_git(args, "", stdout_callback, stderr_callback, quiet_mode);
}

// returns (0, SHA) if ref is resolved
// (2, "") if ref is not found (then ref is invalid, can be an SHA1)
// (other, ::) on error
tuple<int, string> git_ls_remote(string_par url, string_par ref);
string git_rev_parse(string_par ref, string_par dir);
void git_clone(vector<string> args);
int git_checkout(vector<string> args, string_par dir);

// returns empty or unique result, fails otherwise
string try_find_unique_ref_by_sha_with_ls_remote(string_par git_url, string_par sha);

enum resolve_ref_status_t
{
    resolve_ref_error,  // git-ls-remote failed
    resolve_ref_success,
    resolve_ref_not_found,  // git-ls-remote was successful but ref was not found and it doesn't
                            // look like an SHA
    resolve_ref_sha_like,   // ref was not found but it's like an SHA
};

// no fail, reports error in result
tuple<resolve_ref_status_t, string> git_resolve_ref_on_remote(string_par git_url, string_par ref);

// returns valid result or fail
// if allow_sha then sha-like unresolved refs will be returned
string must_git_resolve_ref_on_remote(string_par git_url, string_par ref, bool allow_sha);

// true if x could be a git SHA1
bool sha_like(string_par x);

struct git_status_result_t
{
    vector<string> lines;
    bool clean_or_untracked_only() const;
};
git_status_result_t git_status(string_par dir, bool branch_tracking = false);
}

#endif
