#include "installdb.h"

#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <nowide/fstream.hpp>

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "misc_utils.h"

CEREAL_CLASS_VERSION(cmakex::installed_pkg_desc_t, 1)
CEREAL_CLASS_VERSION(cmakex::installed_pkg_desc_t::depends_item_t, 1)

namespace cmakex {

namespace fs = filesystem;

#define A(X) cereal::make_nvp(#X, m.X)

template <class Archive>
void serialize(Archive& archive, installed_pkg_desc_t::depends_item_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(pkg_name), A(source));
}

template <class Archive>
void serialize(Archive& archive, installed_pkg_desc_t& m, uint32_t version)
{
    THROW_UNLESS(version == 1);
    archive(A(name), A(git_url), A(git_sha), A(source_dir), A(depends), A(cmake_args), A(configs));
}

#undef A

InstallDB::InstallDB(string_par binary_dir)
    : dbpath(cmakex_config_t(binary_dir).cmakex_dir + "/" + "installed")
{
    if (!fs::exists(dbpath))
        fs::create_directories(dbpath);  // must be able to create the path
}

maybe<installed_pkg_desc_t> InstallDB::try_get_installed_pkg_desc(string_par pkg_name) const
{
    auto path = installed_pkg_desc_path(pkg_name);
    nowide::ifstream f(path);
    if (!f.good())
        return nothing;
    string what;
    try {
        // otherwise it must succeed
        cereal::JSONInputArchive a(f);
        maybe<installed_pkg_desc_t> r(in_place);
        a(*r);
        return r;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't read installed package descriptor \"%s\", reason: %s", path.c_str(),
           what.c_str());
    // never here
    return nothing;
}

void InstallDB::put_installed_pkg_desc(const installed_pkg_desc_t& p)
{
    auto path = installed_pkg_desc_path(p.name);
    nowide::ofstream f(path, std::ios_base::trunc);
    if (!f.good())
        throwf("Can't open installed package descriptor \"%s\" for writing.", path.c_str());
    string what;
    try {
        cereal::JSONOutputArchive a(f);
        a(p);
        f.close();
        return;
    } catch (const exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception.";
    }
    throwf("Can't write installed package descriptor \"%s\", reason: %s", path.c_str(),
           what.c_str());
    // never here
}

string InstallDB::installed_pkg_desc_path(string_par pkg_name) const
{
    return dbpath + "/" + pkg_name.c_str() + ".json";
}
}
