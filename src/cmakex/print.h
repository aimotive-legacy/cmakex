#ifndef PRINT_8327683
#define PRINT_8327683

#include <cstdarg>
#include <cstdio>

#include <adasworks/sx/config.h>
#include "using-decls.h"

namespace cmakex {

class OutErrMessages;

void log_info(const char* s, ...) AW_PRINTFLIKE(1, 2);
void log_verbose(const char* s, ...) AW_PRINTFLIKE(1, 2);
void log_info();
void log_info_framed_message(string_par msg);
void log_warn(const char* s, ...) AW_PRINTFLIKE(1, 2);
void log_error(const char* s, ...) AW_PRINTFLIKE(1, 2);
void log_error_errno(const char* s, ...) AW_PRINTFLIKE(1, 2);
void log_fatal(const char* s, ...) AW_PRINTFLIKE(1, 2);

string string_exec(string_par command,
                   const vector<string>& args,
                   string_par working_directory = "");
string log_exec(string_par command, const vector<string>& args, string_par working_directory = "");
string current_datetime_string_for_log();

// saves log from oem to specified location and prints informational message
// if result!=EXIT_SUCCESS also prints to console
void save_log_from_oem(string_par command_line,
                       int result,
                       const OutErrMessages& oem,
                       string_par log_dir,
                       string_par log_filename);

// string datetime_string_for_log(Poco::DateTime dt);
string current_datetime_string_for_log();
// string datetime_string_for_log(std::chrono::system_clock::time_point x);
void log_datetime();
extern bool g_verbose;
}

#endif
