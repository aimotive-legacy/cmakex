#ifndef PARS_029347234
#define PARS_029347234

#include "using-decls.h"

namespace cmakex {
struct cmakex_pars_t
{
    enum subcommand_t
    {
        subcommand_invalid,
        subcommand_cmake_steps
    } subcommand = subcommand_invalid;

    bool c = false;
    bool b = false;
    bool t = false;
    vector<string> configs;
    string binary_dir;
    string source_dir;
    vector<string> config_args;
    vector<string> build_args;
    vector<string> native_tool_args;
    vector<string> build_targets;
    bool config_args_besides_binary_dir = false;
};
}

#endif
