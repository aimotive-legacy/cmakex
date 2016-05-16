#include "cmakexengine.h"

#include <adasworks/sx/check.h>

#include "filesystem.h"

namespace cmakex {

namespace fs = filesystem;

#define BEGINEND(X) (X).begin(), (X).end()

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
void invoke(string_par c, const vector<string>& args)
{
    CHECK(false);  // todo
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

        // add a single, neutral config, if no configs specified
        auto configs = pars.configs;
        if (configs.empty())
            configs = {""};

        // if need to build but no build targets specified, add a single, default build target
        auto build_targets = pars.build_targets;
        if (pars.b && build_targets.empty())
            build_targets = {""};

        for (auto& config : configs) {
            // configure step
            if (pars.c) {
                vector<string> args;
                if (!config.empty()) {
                    args.emplace_back(string("-DCMAKE_BUILD_TYPE=") + config);
                    args.insert(args.end(), BEGINEND(pars.config_args));
                    invoke("cmake", args);
                } else if (pars.config_args_besides_binary_dir) {
                    fprintf(stderr,
                            "You specified args for the cmake configuration step besides binary "
                            "dir:\n");
                    fprintf(stderr, "\t%s\n", join(pars.config_args, " ").c_str());
                    fprintf(stderr, "but the 'c' option is missing from the command word\n");
                    exit(EXIT_FAILURE);
                }
            }

            // build step
            for (auto& target : build_targets) {
                vector<string> args = {string("--build")};
                CHECK(!pars.binary_dir.empty());
                args.emplace_back(pars.binary_dir);
                args.insert(args.end(), BEGINEND(pars.build_args));
                if (!target.empty()) {
                    args.emplace_back("--target");
                    args.emplace_back(target);
                }
                if (!config.empty()) {
                    args.emplace_back("--config");
                    args.emplace_back(config);
                }
                args.insert(args.end(), BEGINEND(pars.native_tool_args));
                invoke("cmake", args);
            }

            // test step
            if (pars.t) {
                fs::current_path(pars.binary_dir);
                vector<string> args;
                if (!config.empty()) {
                    args.emplace_back("-C");
                    args.emplace_back(config);
                }
                invoke("ctest", args);
            }
        }  // for configs
    }
    const cmakex_pars_t pars;
};

unique_ptr<CMakeXEngine> CMakeXEngine::create(const cmakex_pars_t& pars)
{
    return unique_ptr<CMakeXEngine>(new CMakeXEngineImpl(pars));
}
}
