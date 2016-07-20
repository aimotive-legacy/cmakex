#ifndef RUN_BUILD_SCRIPT_239874
#define RUN_BUILD_SCRIPT_239874

#include "installdb.h"

namespace cmakex {

using pkg_desc_map_t = std::map<string, pkg_desc_t>;

struct deps_recursion_wsp_t
{
    struct pkg_t
    {
        pkg_desc_t planned_desc;    // pkg_desc planned after this deps recursion
        bool just_cloned = false;   // cloned in this run of install_deps_phase_one
        string unresolved_git_tag;  // the original, unresolved GIT_TAG, for error messages
    };

    vector<string> requester_stack;
    vector<string> build_order;
    std::map<string, pkg_t> pkg_map;
};

// returns packages encountered during the recursion
vector<string> install_deps_phase_one(
    string_par binary_dir,              // main project binary dir
    string_par source_dir,              // the package's source dir (or the main source dir)
    vector<string> request_deps,        // dependency list from request, will be
                                        // overridden by deps_script_file if it
                                        // exists
    const vector<string>& config_args,  // request config args, will be applied to all deps
    const vector<string>& configs,      // requested configurations, can be overridden per package
    bool strict_commits,  // if true only specified commits will be accepted. If false previously
                          // cloned repos will be accepted even if they don't satisfy the GIT_TAG
                          // requirement
    deps_recursion_wsp_t& wsp);

// returns packages encountered during the recursion
vector<string> run_deps_add_pkg(const vector<string>& args,
                                string_par binary_dir,
                                const vector<string>& configs_args,
                                const vector<string>& configs,
                                bool strict_commits,
                                deps_recursion_wsp_t& wsp);
}

#endif
