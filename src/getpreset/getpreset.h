#ifndef GETPRESET_2039470234
#define GETPRESET_2039470234

#include <string>
#include <tuple>
#include <vector>

#include <adasworks/sx/string_par.h>

namespace libgetpreset {
using adasworks::sx::string_par;
// path_name can be preset-name (CMAKEX_PRESET_FILE env var contains the file)
// or a string of the for path/preset-name
// Returns
//- single-item vector or vector-of-args (for cmake field)
//- resolved file path
//- resolved preset-name
std::tuple<std::vector<std::string>, std::string, std::string> getpreset(string_par path_name,
                                                                         string_par field);
}

#endif
