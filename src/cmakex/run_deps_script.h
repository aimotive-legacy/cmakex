#ifndef RUN_BUILD_SCRIPT_239874
#define RUN_BUILD_SCRIPT_239874

#include "installdb.h"
#include "pars.h"

namespace cmakex {

using pkg_request_map_t = std::map<string, pkg_request_t>;

struct deps_recursion_wsp_t
{
    pkg_request_map_t pkg_request_map;
    vector<string> requester_stack;
    vector<string> build_order;
};

// returns packages encountered during the recursion
vector<string> run_deps_script(string binary_dir,
                               string deps_script_file,
                               const vector<string>& config_args,
                               const vector<string>& configs,
                               bool strict_commits,
                               deps_recursion_wsp_t& wsp);

// returns packages encountered during the recursion
vector<string> run_deps_add_pkg(const vector<string>& args,
                                string binary_dir,
                                const vector<string>& configs,
                                bool strict_commits,
                                deps_recursion_wsp_t& wsp);
}

#endif
