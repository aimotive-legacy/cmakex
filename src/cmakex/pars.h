#ifndef PARS_029347234
#define PARS_029347234

#include "using-decls.h"

namespace cmakex {

enum source_descriptor_kind_t
{
    source_descriptor_invalid,
    source_descriptor_build_script,
    source_descriptor_cmakelists_dir
};

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
    bool binary_dir_valid = false;
    string binary_dir;
    source_descriptor_kind_t source_desc_kind;
    string source_desc;          // directory containing CMakeLists.txt or a *.cmake build script
    vector<string> config_args;  // not including the source and binary dir flags or paths
    vector<string> build_args;
    vector<string> native_tool_args;
    vector<string> build_targets;
    bool config_args_besides_binary_dir = false;
    vector<string> add_pkgs;
};
}

#endif
