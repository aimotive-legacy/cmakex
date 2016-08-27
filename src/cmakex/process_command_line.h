#ifndef PROCESS_COMMAND_LINE_23049732
#define PROCESS_COMMAND_LINE_23049732

#include "cmakex-types.h"

namespace cmakex {

command_line_args_cmake_mode_t process_command_line_1(int argc, char* argv[]);
tuple<processed_command_line_args_cmake_mode_t, cmakex_cache_t> process_command_line_2(
    const command_line_args_cmake_mode_t& args);
}

#endif
