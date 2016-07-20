#ifndef BUILD_23984623
#define BUILD_23984623

#include "cmakex-types.h"
#include "using-decls.h"

namespace cmakex {
// build the referenced package (project)
// it is cloned out, need to be built with options specified in request
void build(string_par binary_dir, const pkg_desc_t& request, string_par config);
}

#endif
