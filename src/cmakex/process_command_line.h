#ifndef PROCESS_COMMAND_LINE_23049732
#define PROCESS_COMMAND_LINE_23049732

#include "cmakex-types.h"

namespace cmakex {

command_line_args_cmake_mode_t process_command_line(int argc, char* argv[]);
processed_command_line_args_cmake_mode_t process_command_line(
    const command_line_args_cmake_mode_t& args);
}

#endif
