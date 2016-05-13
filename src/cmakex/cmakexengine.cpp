#include "cmakexengine.h"

#include <adasworks/sx/check.h>

namespace cmakex {

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
    }
    const cmakex_pars_t pars;
};

unique_ptr<CMakeXEngine> CMakeXEngine::create(const cmakex_pars_t& pars)
{
    return unique_ptr<CMakeXEngine>(new CMakeXEngineImpl(pars));
}
}