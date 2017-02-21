#ifndef RUN_BUILD_SCRIPT_239874
#define RUN_BUILD_SCRIPT_239874

#include <set>

#include "installdb.h"

namespace cmakex {

struct manifest_of_config_t
{
    string git_url;
    string git_tag;  // only for comparison
    string git_tag_and_comment;
    string depends_maybe_commented;
    bool depends_from_script;
    string source_dir;
    string cmake_args;
    string c_sha;
    string toolchain_sha;
    bool operator==(const manifest_of_config_t& y) const
    {
        return git_url == y.git_url && git_tag == y.git_tag &&
               depends_maybe_commented == y.depends_maybe_commented && source_dir == y.source_dir &&
               cmake_args == y.cmake_args;
    }
    bool operator!=(const manifest_of_config_t& y) const { return !(*this == y); }
};

struct deps_recursion_wsp_t
{
    struct per_config_data
    {
        vector<string> build_reasons;
        vector<string> cmake_args_to_apply;
        final_cmake_args_t tentative_final_cmake_args;
    };
    using manifests_per_config_t = std::map<config_name_t, manifest_of_config_t>;
    struct pkg_t
    {
        pkg_t(const pkg_request_t& req) : request(req) {}
        pkg_request_t request;
        bool just_cloned = false;  // cloned in this run of install_deps_phase_one
        string resolved_git_tag;
        string found_on_prefix_path;  // if non-empty this package has been found on a prefix path
        std::map<config_name_t, per_config_data> pcd;
        bool building_now = false;
        bool dependencies_from_script = false;  // true if dependencies read from deps.cmake which
                                                // overrides all DEPENDS specifications
        manifests_per_config_t manifests_per_config;
    };

    vector<string> requester_stack;
    vector<string> build_order;
    std::map<string, pkg_t> pkg_map;
    std::map<string, pkg_request_t> pkg_def_map;
    std::set<string>
        pkgs_to_process;  // not unordered set because we prefer cross-platform determinism
    bool force_build = false;
    bool clear_downloaded_include_files = false;
    bool update = false;
    bool update_can_leave_branch = false;
    bool update_stop_on_error = true;
    bool update_can_reset = false;
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
// the boolean indicates the dependencies has been read from deps script (and not a DEPENDS
// argument)
tuple<idpo_recursion_result_t, bool> install_deps_phase_one(
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
