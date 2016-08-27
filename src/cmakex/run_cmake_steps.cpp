#include "run_cmake_steps.h"

#include <adasworks/sx/check.h>
#include <adasworks/sx/format.h>

#include "filesystem.h"

#include "build.h"
#include "misc_utils.h"
#include "out_err_messages.h"
#include "print.h"

namespace cmakex {

namespace fs = filesystem;
namespace sx = adasworks::sx;

void run_cmake_steps(const processed_command_line_args_cmake_mode_t& pars,
                     const cmakex_cache_t& cmakex_cache)
{
    CHECK(!pars.source_dir.empty());
    CHECK(!pars.binary_dir.empty());
    CHECK(!pars.configs.empty());

    auto main_tic = high_resolution_clock::now();
    log_info("Started at %s", current_datetime_string_for_log().c_str());

    // if need to build but no build targets specified, add a single, default build target
    auto build_targets = pars.build_targets;
    if (pars.flag_b && build_targets.empty())
        build_targets = {""};

    bool force_config_step_now = !pars.cmake_args.empty() || pars.flag_c;
    for (auto& config_str : pars.configs) {
        config_name_t config(config_str);
        log_info("Building: '%s'", config.get_prefer_NoConfig().c_str());
        build(pars.binary_dir, "", pars.source_dir, pars.cmake_args, config, build_targets,
              force_config_step_now, cmakex_cache);

        if (cmakex_cache.multiconfig_generator)
            force_config_step_now = false;

    }  // for configs
    log_info("Finished at %s, elapsed %s", current_datetime_string_for_log().c_str(),
             sx::format_duration(dur_sec(high_resolution_clock::now() - main_tic).count()).c_str());
}
}
