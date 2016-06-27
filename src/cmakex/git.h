#ifndef GIT_23849382
#define GIT_23849382

#include "out_err_messages.h"
#include "using-decls.h"

namespace cmakex {

// find git with cmake's find_package(Git), on failure returns "git"
string find_git_or_return_git();

int exec_git(const vector<string>& args,
             string_par working_directory,
             exec_process_output_callback_t stdout_callback = nullptr,
             exec_process_output_callback_t stderr_callback = nullptr);

inline int exec_git(const vector<string>& args,
                    exec_process_output_callback_t stdout_callback = nullptr,
                    exec_process_output_callback_t stderr_callback = nullptr)
{
    return exec_git(args, "", stdout_callback, stderr_callback);
}

// returns (0, SHA) if ref is resolved
// (2, "") if ref is not found (then ref is invalid, can be an SHA1)
// (other, ::) on error
tuple<int, string> git_ls_remote(string_par url, string_par ref);
string git_rev_parse(string_par ref, string_par dir);
void git_clone(vector<string> args);
int git_checkout(vector<string> args, string_par dir);
string try_resolve_sha_to_tag(string_par git_url, string_par sha);
enum resolve_ref_status_t
{
    resolve_ref_error,  // git-ls-remote failed
    resolve_ref_success,
    resolve_ref_not_found,  // git-ls-remote was successful but ref was not found and it doesn't
                            // look like an SHA
    resolve_ref_sha_like,   // ref was not found but it's like an SHA
};
tuple<resolve_ref_status_t, string> git_resolve_ref_on_remote(string_par git_url, string_par ref);
// true if x could be a git SHA1

bool sha_like(string_par x);
}

#endif
