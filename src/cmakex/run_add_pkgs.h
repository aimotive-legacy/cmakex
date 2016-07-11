#ifndef RUN_ADD_PKGS_233264
#define RUN_ADD_PKGS_233264

#include "cmakex-types.h"

namespace cmakex {
void run_add_pkgs(const cmakex_pars_t& pars);
pkg_request_t pkg_request_from_arg_str(const string& pkg_arg_str);
pkg_request_t pkg_request_from_args(const vector<string>& pkg_args);
}

#endif
