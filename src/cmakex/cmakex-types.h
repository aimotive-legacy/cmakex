#ifndef CMAKEX_TYPES_29034023
#define CMAKEX_TYPES_29034023

#include "using-decls.h"

namespace cmakex {

enum git_tag_kind_t
{
    // order is important
    git_tag_is_not_sha,    // it's not sha-like
    git_tag_could_be_sha,  // it's sha-like but not checked
    git_tag_must_be_sha,   // it's sha-like and server returned not found
    git_tag_is_sha,        // we initialized it with a known sha
};

bool sha_like(string_par x);

inline git_tag_kind_t initial_git_tag_kind(string git_tag)
{
    return sha_like(git_tag) ? git_tag_could_be_sha : git_tag_is_not_sha;
}

struct pkg_clone_pars_t
{
    string git_url;
    string git_tag;
    bool git_shallow = true;  // if false, clone only the requested branch at depth=1
};

// pkg request is what comes from the registry (to be implemented)
// and from the local package definition script (the ExternalProject-like
// parameters)
struct pkg_request_t
{
    string name;
    pkg_clone_pars_t clone_pars;
    string source_dir;
    vector<string> depends;
    vector<string> cmake_args;
    vector<string> configs;
};
}

#endif
