#include "print.h"

#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Timezone.h"

#include "filesystem.h"
#include "out_err_messages.h"

namespace cmakex {

namespace fs = filesystem;

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
                case ';':
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

void save_log_from_oem(const OutErrMessages& oem, string_par log_dir, string_par log_filename)
{
    if (!fs::is_directory(log_dir.c_str())) {
        string msg;
        try {
            fs::create_directories(log_dir.c_str());
        } catch (const exception& e) {
            msg = e.what();
        } catch (...) {
            msg = "unknown exception";
        }
        if (!msg.empty()) {
            print_err("Can't create directory for logs (\"%s\"), reason: %s.", log_dir.c_str(),
                      msg.c_str());
            return;
        }
    }
    string log_path = log_dir.str() + "/" + log_filename.c_str();
    FILE* f = fopen(log_path.c_str(), "wt");
    if (!f) {
        print_err("Can't open log file for writing: \"%s\".", log_path.c_str());
        return;
    }
    string text;
    for (int i = 0; i < oem.size(); ++i) {
        auto msg = oem.at(i);
        text = msg.text;
        int nlcount = 0;
        int idx = (int)text.size() - 1;
        for (; idx >= 0; --idx) {
            char c = text[idx];
            if (c == 13)
                continue;
            if (c == 10) {
                ++nlcount;
                continue;
            }
            break;
        }
        ++idx;
        fprintf(f, "%s[%.2f] %.*s\n",
                msg.source == out_err_message_base_t::source_stdout ? "    " : "ERR ", msg.t, idx,
                msg.text.c_str());
    }
    fclose(f);
    print_out("Log saved to \"%s\".", log_path.c_str());
}
}