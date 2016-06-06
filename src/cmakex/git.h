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

int exec_git(const vector<string>& args,
             exec_process_output_callback_t stdout_callback = nullptr,
             exec_process_output_callback_t stderr_callback = nullptr)
{
    return exec_git(args, "", stdout_callback, stderr_callback);
}

tuple<int, string> git_ls_remote(string_par url, string_par ref);
string git_rev_parse_head(string_par dir);
}

#endif
