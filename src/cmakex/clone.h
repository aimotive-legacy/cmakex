#ifndef CLONE_20394702934
#define CLONE_20394702934

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {

enum pkg_clone_dir_status_t
{
    pkg_clone_dir_invalid,
    pkg_clone_dir_doesnt_exist,
    pkg_clone_dir_empty,
    pkg_clone_dir_nonempty_nongit,
    pkg_clone_dir_git
};

// returns the package's clone dir's status, SHA, if git
tuple<pkg_clone_dir_status_t, string> pkg_clone_dir_status(string_par binary_dir,
                                                           string_par pkg_name);

// executes git-clone
// cp's git_tag_kind might get updated
void clone(string_par pkg_name, pkg_clone_pars_t& cp, string_par binary_dir);

// cp.git_tag must be an SHA
void make_sure_exactly_this_sha_is_cloned_or_fail(string_par pkg_name,
                                                  const pkg_clone_pars_t& cp,
                                                  string_par binary_dir);

// throws if strict, warns otherwise
void make_sure_exactly_this_git_tag_is_cloned(string_par pkg_name,
                                              const pkg_clone_pars_t& cp,
                                              string_par binary_dir,
                                              bool strict);
}

#endif
