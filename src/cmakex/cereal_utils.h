#ifndef CEREAL_UTILS_20934
#define CEREAL_UTILS_20934

#include <cereal/archives/json.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <nowide/fstream.hpp>

#include <adasworks/sx/check.h>

#include "misc_utils.h"
#include "using-decls.h"

namespace cmakex {

template <class T>
void save_json_output_archive(string_par path, const T& x)
{
    nowide::ofstream f(path, std::ios_base::trunc);
    if (!f.good())
        throwf("Can't open \"%s\" for writing.", path.c_str());
    string what;
    try {
        cereal::JSONOutputArchive a(f);
        a(x);
        return;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't write \"%s\", reason: %s", path.c_str(), what.c_str());
}

template <class T>
void load_json_input_archive(string_par path, T& out)
{
    nowide::ifstream f(path);
    if (!f.good())
        throwf("Can't open existing file \"%s\" for reading.", path.c_str());
    string what;
    try {
        // otherwise it must succeed
        cereal::JSONInputArchive a(f);
        a(out);
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't read \"%s\", reason: %s", path.c_str(), what.c_str());
}
}

#endif
