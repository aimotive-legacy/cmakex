#ifndef INSTALL_DEPS_PHASE_TWO_23429834
#define INSTALL_DEPS_PHASE_TWO_23429834

#include "install_deps_phase_one.h"

namespace cmakex {

// iterarate build order
// check each item if it must be built or not
// build & install if needed
void install_deps_phase_two(string_par binary_dir,
                            deps_recursion_wsp_t& wsp,
                            bool force_config_step);
}

#endif
