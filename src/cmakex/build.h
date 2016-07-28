#ifndef BUILD_23984623
#define BUILD_23984623

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {
// build the referenced package (project)
// it is cloned out, need to be built with options specified in request
void build(string_par binary_dir,
           const pkg_desc_t& request,
           string_par config,
           bool first_config,      // this function will be called in loop which iterates over the
                                   // configurations requested
           bool force_config_step  // config even there are no cmake_args different to what are in
                                   // the cache
           );
}

#endif
