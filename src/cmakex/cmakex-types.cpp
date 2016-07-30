#include "cmakex-types.h"

namespace cmakex {
bool operator==(const cmakex_cache_t& x, const cmakex_cache_t& y)
{
    return x.valid == y.valid && x.home_directory == y.home_directory &&
           x.multiconfig_generator == y.multiconfig_generator &&
           x.per_config_bin_dirs == y.per_config_bin_dirs;
}
}