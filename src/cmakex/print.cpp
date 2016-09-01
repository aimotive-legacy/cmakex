#include "print.h"

#include <nowide/cstdio.hpp>

#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Timezone.h>

#include "filesystem.h"
#include "misc_utils.h"
#include "out_err_messages.h"

namespace cmakex {

namespace fs = filesystem;

bool g_verbose = false;

void log_info()
{
    printf("--\n");
    fflush(stdout);
}

void log_info(const char* s, ...)
{
    printf("-- ");
    va_list ap;
    va_start(ap, s);
    vprintf(s, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}
void log_info_framed_message(string_par msg)
{
    auto stars = string(msg.size() + 4, '*');
    log_info("%s", stars.c_str());
    log_info("* %s *", msg.c_str());
    log_info("%s", stars.c_str());
}

void log_warn(const char* s, ...)
{
    printf("-- WARNING: ");
    va_list ap;
    va_start(ap, s);
    vprintf(s, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

void log_error(const char* s, ...)
{
    fprintf(stderr, "cmakex: ERROR: ");
    va_list ap;
    va_start(ap, s);
    vfprintf(stderr, s, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
void log_fatal(const char* s, ...)
{
    fprintf(stderr, "cmakex: [FATAL] ");
    va_list ap;
    va_start(ap, s);
    vfprintf(stderr, s, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}
void log_error_errno(const char* s, ...)
{
    int was_errno = errno;
    fprintf(stderr, "cmakex: [ERROR]");
    va_list ap;
    va_start(ap, s);
    vfprintf(stderr, s, ap);
    va_end(ap);
    if (was_errno)
        fprintf(stderr, ", reason: %s (%d)\n", strerror(was_errno), was_errno);
    else
        fprintf(stderr, ".\n");
}

string escape_arg(string_par arg)
{
    string t;
    bool quoted = false;
    for (const char* cs = arg.c_str(); *cs; ++cs) {
        auto c = *cs;
        switch (c) {
            case ' ':
            case ';':
                if (!quoted) {
                    quoted = true;
                    t.insert(t.begin(), '"');
                }
                t.push_back(c);
                break;
#ifndef _WIN32
            case '\\':
                t.append("\\\\");
                break;
#endif
            case '"':
                t.append("\\\"");
                break;
            default:
                t.push_back(c);
        }
    }
    if (quoted)
        t.push_back('"');
    return t;
}

string string_exec(string_par command, const vector<string>& args, string_par working_directory)
{
    string cd_prefix;
    if (!working_directory.empty())
        cd_prefix = stringf("cd %s && ", escape_arg(working_directory).c_str());
    string u = escape_arg(command);
    for (auto& s : args) {
        u.push_back(' ');
        u.append(escape_arg(s));
    }
    return stringf("$ %s%s", cd_prefix.c_str(), u.c_str());
}
string log_exec(string_par command, const vector<string>& args, string_par working_directory)
{
    auto r = string_exec(command, args, working_directory);
    printf("%s\n", r.c_str());
    return r;
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

struct slf_helper_t
{
    slf_helper_t(int result, FILE* f) : result(result), f(f) {}
    int result;
    FILE* f;
};

void slf_printf(slf_helper_t& h, string_par s)
{
    fprintf(h.f, "%s", s.c_str());
    if (h.result != EXIT_SUCCESS)
        printf("%s", s.c_str());
}

void save_log_from_oem(string_par command_line,
                       int result,
                       const OutErrMessages& oem,
                       string_par log_dir,
                       string_par log_filename)
{
#ifdef _MSC_VER
    _set_printf_count_output(1);
#endif

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
            log_error("Can't create directory for logs (\"%s\"), reason: %s.", log_dir.c_str(),
                      msg.c_str());
            return;
        }
    }
    string log_path = log_dir.str() + "/" + log_filename.c_str();
    auto maybe_f = try_fopen(log_path, "w");
    if (!maybe_f) {
        log_error_errno("Can't open log file for writing: \"%s\"", log_path.c_str());
        return;
    }
    auto f = move(*maybe_f);

    slf_helper_t h(result, f.stream());

    slf_printf(
        h, stringf("Started at %s\n%s\n", datetime_string_for_log(oem.start_system_time()).c_str(),
                   command_line.c_str()));
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
                    slf_printf(h, stringf("%s[%.2f] %n%.*s\n", stderr_marker, msg.t, &indent,
                                          x1 - x0, msg.text.c_str() + x0));
                else {
                    assert(indent - c_stderr_marker_length <= strlen(c_spaces));
                    slf_printf(
                        h, stringf("%s%.*s%.*s\n", stderr_marker, indent - c_stderr_marker_length,
                                   c_spaces, x1 - x0, msg.text.c_str() + x0));
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
                    slf_printf(h, stringf("%s\n", stderr_marker));
                else
                    slf_printf(h, "\n");
            }

            x0 = x1;
        }
    }
    slf_printf(h,
               stringf("Finished at %s\n", datetime_string_for_log(oem.end_system_time()).c_str()));

    if (result)
        log_info("Log saved to \"%s\".", /*prefix_msg.c_str(), */ log_path.c_str());
}

void log_datetime()
{
    log_info("%s", current_datetime_string_for_log().c_str());
}
}
