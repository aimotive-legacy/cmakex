#include "print.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Timezone.h"

namespace cmakex {
void vprint(FILE* f, const char* s, va_list vl)
{
    fprintf(f, "cmakex: ");
    vfprintf(f, s, vl);
    fprintf(f, "\n");
}
void print_out(const char* s, ...)
{
    va_list ap;
    va_start(ap, s);
    vprint(stdout, s, ap);
    va_end(ap);
}
void print_err(const char* s, ...)
{
    va_list ap;
    va_start(ap, s);
    vprint(stderr, s, ap);
    va_end(ap);
}
void log_exec(string_par command, const vector<string>& args)
{
    string u = command.str();
    string t;
    for (auto& s : args) {
        t.clear();
        bool quoted = false;
        for (auto c : s) {
            switch (c) {
                case ' ':
                    if (!quoted) {
                        quoted = true;
                        t.insert(t.begin(), '"');
                    }
                    break;
                case '\\':
                    t.append("\\\\");
                    break;
                case '"':
                    t.append("\\\"");
                    break;
                default:
                    t.push_back(c);
            }
        }
        if (quoted)
            t.push_back('"');
        u.push_back(' ');
        u.append(t);
    }
    print_out("$ %s", u.c_str());
}
string current_datetime_string_for_log()
{
    Poco::DateTime dt;
    dt.makeLocal(Poco::Timezone::tzd());
    return Poco::DateTimeFormatter::format(dt, Poco::DateTimeFormat::RFC1123_FORMAT,
                                           Poco::Timezone::tzd());
}
}