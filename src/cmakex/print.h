#ifndef PRINT_8327683
#define PRINT_8327683

#include <cstdarg>
#include <cstdio>

#include <adasworks/sx/config.h>
#include "using-decls.h"

namespace cmakex {

class OutErrMessages;

void log_info(const char* s, ...) AW_PRINTFLIKE(1, 2);
void log_warn(const char* s, ...) AW_PRINTFLIKE(1, 2);
void log_error(const char* s, ...) AW_PRINTFLIKE(1, 2);
void log_error_errno(const char* s, ...) AW_PRINTFLIKE(1, 2);

void log_exec(string_par command, const vector<string>& args);
string current_datetime_string_for_log();
void save_log_from_oem(const OutErrMessages& oem, string_par log_dir, string_par log_filename);
}

#endif
