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
                        t.push_back(c);
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
string datetime_string_for_log(Poco::DateTime dt)
{
    dt.makeLocal(Poco::Timezone::tzd());
    return Poco::DateTimeFormatter::format(dt, Poco::DateTimeFormat::RFC1123_FORMAT,
                                           Poco::Timezone::tzd());
}
string current_datetime_string_for_log()
{
    return datetime_string_for_log(Poco::DateTime());
}
string datetime_string_for_log(std::chrono::system_clock::time_point x)
{
    auto t = std::chrono::system_clock::to_time_t(x);
    auto ts = Poco::Timestamp::fromEpochTime(t);
    auto dt = Poco::DateTime(ts);
    return datetime_string_for_log(dt);
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

    fprintf(f, "Started at %s\n", datetime_string_for_log(oem.start_system_time()).c_str());
    const char c_line_feed = 10;
    const char c_carriage_return = 13;
    const int c_stderr_marker_length = 4;
    // longest timestamp must fit into c_spaces
    //                      [12345678901.23]
    const char* c_spaces = "                                ";
    for (int i = 0; i < oem.size(); ++i) {
        auto msg = oem.at(i);
        int xs = msg.text.size();
        int x0 = 0;
        int indent = -1;
        const char* stderr_marker =
            msg.source == out_err_message_base_t::source_stdout ? "    " : "ERR ";
        assert(c_stderr_marker_length == strlen(stderr_marker));
        for (; x0 < xs;) {
            int x1 = x0;
            // find the next newline-free section
            while (x1 < xs && msg.text[x1] != c_line_feed && msg.text[x1] != c_carriage_return)
                ++x1;
            // print the newline-free section
            if (x0 < x1) {
                if (indent < 0)
                    fprintf(f, "%s[%.2f] %n%.*s\n", stderr_marker, msg.t, &indent, x1 - x0,
                            msg.text.c_str() + x0);
                else {
                    assert(indent - c_stderr_marker_length <= strlen(c_spaces));
                    fprintf(f, "%s%.*s%.*s\n", stderr_marker, indent - c_stderr_marker_length,
                            c_spaces, x1 - x0, msg.text.c_str() + x0);
                }
            }
            // find the next newline section
            x0 = x1;
            int newline_count = 0;
            for (; x1 < xs; ++x1) {
                auto c = msg.text[x1];
                if (c == c_line_feed)
                    ++newline_count;
                else if (c != c_carriage_return)
                    break;
            }
            // print at most one extra newline
            if (newline_count > 1) {
                if (msg.source == out_err_message_base_t::source_stderr)
                    fprintf(f, "%s\n", stderr_marker);
                else
                    fprintf(f, "\n");
            }

            x0 = x1;
        }
    }
    fprintf(f, "Finished at %s\n", datetime_string_for_log(oem.end_system_time()).c_str());
    fclose(f);
    print_out("Log saved to \"%s\".", log_path.c_str());
}
}
