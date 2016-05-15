#include "cmakexengine.h"

#include <adasworks/sx/check.h>

namespace cmakex {

namespace {
string join(const vector<string>& v, const string& s)
{
    if (v.empty())
        return string();
    size_t l((v.size() - 1) * s.size());
    for (auto& x : v)
        l += x.size();
    string r;
    r.reserve(l);
    r += v.front();
    for (int i = 1; i < v.size(); ++i) {
        r += s;
        r += v[i];
    }
    return r;
}
}

class CMakeXEngineImpl : public CMakeXEngine
{
public:
    CMakeXEngineImpl(const cmakex_pars_t& pars) : pars(pars) {}
    virtual void run() override
    {
        switch (pars.subcommand) {
            case cmakex_pars_t::subcommand_cmake_steps:
                run_cmake_steps();
                break;
            default:
                CHECK(false);
        }
    }

private:
    void run_cmake_steps()
    {
        if (!pars.source_dir.empty())
            printf("CMAKE_SOURCE_DIR: %s\n", pars.source_dir.c_str());
        printf("CMAKE_BINARY_DIR: %s\n", pars.binary_dir.c_str());
        if (!pars.build_targets.empty())
            printf("targets: %s\n", join(pars.build_targets, " ").c_str());
        if (!pars.configs.empty())
            printf("configurations: %s\n", join(pars.configs, " ").c_str());
    }
    const cmakex_pars_t pars;
};

unique_ptr<CMakeXEngine> CMakeXEngine::create(const cmakex_pars_t& pars)
{
    return unique_ptr<CMakeXEngine>(new CMakeXEngineImpl(pars));
}
}
