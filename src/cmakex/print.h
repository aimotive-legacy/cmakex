#ifndef PRINT_8327683
#define PRINT_8327683

#include <cstdarg>
#include <cstdio>

#include <adasworks/sx/config.h>
#include "using-decls.h"

namespace cmakex {
inline void vprint(FILE* f, const char* s, va_list vl) AW_PRINTFLIKE(2, 0);
inline void vprint(FILE* f, const char* s, va_list vl)
{
    fprintf(f, "[cmakex] ");
    vfprintf(f, s, vl);
    fprintf(f, "\n");
}
inline void print_out(const char* s, ...) AW_PRINTFLIKE(1, 2);
inline void print_out(const char* s, ...)
{
    va_list ap;
    va_start(ap, s);
    vprint(stdout, s, ap);
    va_end(ap);
}
inline void print_err(const char* s, ...) AW_PRINTFLIKE(1, 2);
inline void print_err(const char* s, ...)
{
    va_list ap;
    va_start(ap, s);
    vprint(stderr, s, ap);
    va_end(ap);
}
void log_exec(string_par command, const vector<string>& args);
string current_datetime_string_for_log();
}

#endif
