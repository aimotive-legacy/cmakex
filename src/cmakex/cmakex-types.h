#ifndef CMAKEX_TYPES_29034023
#define CMAKEX_TYPES_29034023

#include "using-decls.h"

namespace cmakex {

struct pkg_clone_pars_t
{
    string git_url;
    string git_tag;
    bool git_tag_is_sha = false;  // false means we don't know
    bool full_clone = false;      // if false, clone only the requested branch at depth=1
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
