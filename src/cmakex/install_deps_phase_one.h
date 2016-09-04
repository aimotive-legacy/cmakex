#ifndef RUN_BUILD_SCRIPT_239874
#define RUN_BUILD_SCRIPT_239874

#include <set>

#include "installdb.h"

namespace cmakex {

struct deps_recursion_wsp_t
{
    struct per_config_data
    {
        vector<string> build_reasons;
        vector<string> cmake_args_to_apply;
        vector<string> tentative_final_cmake_args;
    };
    struct pkg_t
    {
        pkg_t(const pkg_request_t& req) : request(req) {}
        pkg_request_t request;
        bool just_cloned = false;  // cloned in this run of install_deps_phase_one
        string resolved_git_tag;
        string found_on_prefix_path;  // if non-empty this package has been found on a prefix path
        std::map<config_name_t, per_config_data> pcd;
    };

    vector<string> requester_stack;
    vector<string> build_order;
    std::map<string, pkg_t> pkg_map;
    std::map<string, pkg_request_t> pkg_def_map;
    std::set<string>
        pkgs_to_process;  // not unordered set because we prefer cross-platform determinism
    bool force_build = false;
    bool clear_downloaded_include_files = false;
};

// install_deps_phase_one recursion result: aggregates certain data below a node in the recursion
// tree
struct idpo_recursion_result_t
{
    vector<string> pkgs_encountered;  // packages encountered during the recursion
    bool building_some_pkg = false;   // if one of those packages are marked to be built

    void clear()
    {
        pkgs_encountered.clear();
        building_some_pkg = false;
    }
    void add(const idpo_recursion_result_t& x);
    void add_pkg(string_par x);

private:
    void normalize();
};

// returns packages encountered during the recursion
idpo_recursion_result_t install_deps_phase_one(
    string_par binary_dir,               // main project binary dir
    string_par source_dir,               // the package's source dir (or the main source dir)
    const vector<string>& request_deps,  // dependency list from request, will be
    // overridden by deps_script_file if it
    // exists
    const vector<string>& global_cmake_args,
    const vector<config_name_t>&
        configs,  // requested configurations, can be overridden per package. Must be
    // a non-empty list of valid configs or a single empty string
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache,  // should be written out if dirty, after the first
    // successful run of the wrapper project
    string_par custom_deps_script_file  // if non-empty, it will be used instead of the default one
                                        // next to CMakeLists.txt
    );

idpo_recursion_result_t run_deps_add_pkg(
    string_par args,
    string_par binary_dir,
    const vector<string>& global_cmake_args,
    const vector<config_name_t>& configs,  // same constraints as for install_deps_phase_one
    deps_recursion_wsp_t& wsp,
    const cmakex_cache_t& cmakex_cache);
}

#endif
