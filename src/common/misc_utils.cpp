#include "misc_utils.h"

#include <cctype>
#include <cerrno>

#include <Poco/SHA1Engine.h>
#include <nowide/cstdio.hpp>

#include <adasworks/sx/check.h>

namespace cmakex {

using adasworks::sx::vstringf;

bool is_one_of(string_par x, initializer_list<const char*> y)
{
    for (auto& i : y) {
        if (x == i)
            return true;
    }
    return false;
}

bool starts_with(string_par x, string_par y)
{
    const auto ys = y.size();
    if (ys == 0)
        return true;
    const auto xs = x.size();
    if (ys > xs)
        return false;
    for (int i = 0; i < ys; ++i) {
        if (x[i] != y[i])
            return false;
    }
    return true;
}

bool istarts_with(string_par x, string_par y)
{
    const auto ys = y.size();
    if (ys == 0)
        return true;
    const auto xs = x.size();
    if (ys > xs)
        return false;
    for (int i = 0; i < ys; ++i) {
        if (std::tolower(x[i]) != std::tolower(y[i]))
            return false;
    }
    return true;
}

bool ends_with(string_par x, string_par y)
{
    const auto ys = y.size();
    if (ys == 0)
        return true;
    const auto xs = x.size();
    if (ys > xs)
        return false;
    auto o = xs - ys;
    for (int i = 0; i < ys; ++i) {
        if (x[o + i] != y[i])
            return false;
    }
    return true;
}

bool starts_with(string_par x, char y)
{
    return !x.empty() && x[0] == y;
}

array_view<const char> butleft(string_par x, int z)
{
    const auto xs = x.size();
    if (xs <= z)
        return {};
    return array_view<const char>(x.c_str() + z, xs - z);
}

string join(const vector<string>& v, const string& s)
{
    if (v.empty())
        return string();
    size_t l((v.size() - 1) * s.size());
    for (auto& x : v)
        l += x.size();
    string r;
    r.reserve(l);
    r += v.front();
    for (int i = 1; i < v.size(); ++i) {
        r += s;
        r += v[i];
    }
    return r;
}

AW_NORETURN void throwf(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    string s = vstringf(format, ap);
    va_end(ap);
    throw std::runtime_error(s);
}

AW_NORETURN void throwf_errno(const char* format, ...)
{
    int was_errno = errno;
    va_list ap;
    va_start(ap, format);
    string s = vstringf(format, ap);
    va_end(ap);
    if (errno)
        s += stringf(", reason: %s (%d)", strerror(was_errno), was_errno);
    else
        s += ".";
    throw std::runtime_error(s);
}

file_t must_fopen(string_par path, string_par mode)
{
    FILE* f = nowide::fopen(path.c_str(), mode.c_str());
    if (!f)
        throwf_errno("Can't open %s in mode \"%s\"", path_for_log(path).c_str(), mode.c_str());
    return file_t(f);
}

maybe<file_t> try_fopen(string_par path, string_par mode)
{
    FILE* f = nowide::fopen(path.c_str(), mode.c_str());
    return f ? just(file_t(f)) : nothing;
}

void must_fprintf(const file_t& f, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    int r = vfprintf(f.stream(), format, ap);
    va_end(ap);
    if (r < 0)
        throwf_errno("Write error in file %s", path_for_log(f.path()).c_str());
}
int fprintf(const file_t& f, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    int r = vfprintf(f.stream(), format, ap);
    va_end(ap);
    return r;
}

string must_fgetline_if_not_eof(const file_t& f)
{
    const int c_bufsize = 1024;
    char buf[c_bufsize];
    string r;
    for (;;) {
        char* p = fgets(buf, c_bufsize, f.stream());
        if (p) {
            r.append(p);
            const char c_line_feed = 10;
            if (!r.empty() && r.back() == c_line_feed) {
                r.pop_back();
                return r;
            }
        } else {
            if (feof(f.stream()))
                return r;
            throwf("Read error in file %s%s", path_for_log(f.path()).c_str(),
                   std::ferror(f.stream()) ? "." : ", ferror is zero.");
        }
    }
}

string strip_trailing_whitespace(string_par x)
{
    string s = x.str();
    while (!x.empty() && iswspace(s.back()))
        s.pop_back();
    return s;
}
vector<string> separate_arguments(string_par x)
{
    bool in_dq = false;
    vector<string> r;
    bool valid = false;
    auto new_empty_if_invalid = [&r, &valid]() {
        if (!valid) {
            r.emplace_back();
            valid = true;
        }
    };
    auto append = [&r, &valid](char c) {
        if (valid)
            r.back() += c;
        else {
            r.emplace_back(1, c);
            valid = true;
        }
    };
    for (const char* c = x.c_str(); *c; ++c) {
        if (in_dq) {
            if (*c == '"')
                in_dq = false;
            else
                append(*c);
        } else {
            if (*c == '"') {
                in_dq = true;
                new_empty_if_invalid();
            } else if (*c == ' ')
                valid = false;
            else
                append(*c);
        }
    }
    return r;
}

std::map<string, vector<string>> parse_arguments(const vector<string>& options,
                                                 const vector<string>& onevalue_args,
                                                 const vector<string>& multivalue_args,
                                                 const vector<string>& args)
{
    std::map<string, vector<string>> r;
    for (int ix = 0; ix < args.size(); ++ix) {
        const auto& arg = args[ix];
        if (is_one_of(arg, options))
            r[arg];
        else if (is_one_of(arg, onevalue_args)) {
            if (++ix >= args.size())
                throwf("Missing argument after \"%s\".", arg.c_str());
            if (r.count(arg) > 0)
                throwf("Single-value argument '%s' specified multiple times.", arg.c_str());
            r[arg] = vector<string>(1, args[ix]);
        } else if (is_one_of(arg, multivalue_args)) {
            for (++ix; ix < args.size(); ++ix) {
                const auto& v = args[ix];
                if (is_one_of(v, options) || is_one_of(v, onevalue_args) ||
                    is_one_of(v, multivalue_args)) {
                    --ix;
                    break;
                }
                r[arg].emplace_back(v);
            }
        } else
            throwf("Invalid option: '%s'.", arg.c_str());
    }
    return r;
}
std::map<string, vector<string>> parse_arguments(const vector<string>& options,
                                                 const vector<string>& onevalue_args,
                                                 const vector<string>& multivalue_args,
                                                 string_par argstr)
{
    return parse_arguments(options, onevalue_args, multivalue_args, separate_arguments(argstr));
}

vector<string> must_read_file_as_lines(string_par path)
{
    auto f = must_fopen(path, "r");
    vector<string> r;

    do
        r.emplace_back(must_fgetline_if_not_eof(f));
    while (!feof(f));

    if (r.back().empty())
        r.pop_back();
    return r;
}
vector<string> split(string_par x, char y)
{
    vector<string> v;
    if (x.empty())
        return v;
    v.emplace_back();
    for (auto c : x) {
        if (c == y)
            v.emplace_back();
        else
            v.back().push_back(c);
    }
    return v;
}

string file_sha(string_par path)
{
    Poco::SHA1Engine e;
    auto f = must_fopen(path, "rb");
    vector<char> buf(1000000);
    size_t bytes_read;
    do {
        bytes_read = fread(buf.data(), 1, buf.size(), f);
        if (ferror(f))
            throwf("Error reading %s for SHA calculation.", path_for_log(path).c_str());
        e.update(buf.data(), bytes_read);
    } while (bytes_read == buf.size());
    return Poco::DigestEngine::digestToHex(e.digest());
}

string string_sha(const string& x)
{
    Poco::SHA1Engine e;
    e.update(x.data(), x.size());
    return Poco::DigestEngine::digestToHex(e.digest());
}

bool tolower_equals(string_par x, string_par y)
{
    const char* i = x.c_str();
    const char* j = y.c_str();
    for (;; ++i, ++j) {
        if (*i == 0 && *j == 0)
            return true;
        if (*i == 0 || *j == 0)
            return false;
        if (std::tolower(*i) != std::tolower(*j))
            return false;
    }
    return true;
}
string trim(string_par x)
{
    const char* c = x.c_str();
    while (*c != 0 && iswspace(*c))
        ++c;
    string r = c;
    while (!r.empty() && iswspace(r.back()))
        r.pop_back();
    return r;
}

void tolower_inplace(string& x)
{
    for (auto& c : x)
        c = std::tolower(c);
}

string tolower(string x)
{
    tolower_inplace(x);
    return x;
}

char system_path_separator()
{
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

void must_write_text(string_par path, string_par content)
{
    FILE* f = fopen(path, "w");
    if (!f)
        throwf("Can't open file for writing: %s", path_for_log(path).c_str());
    int r = fprintf(f, "%s", content.c_str());
    if (r <= 0) {
        int e = errno;
        throwf("Failed to write file: %s, errno: %d, reason: %s", path_for_log(path).c_str(), e,
               strerror(e));
    }
    fclose(f);
}
string pkg_for_log(string_par pkg_name)
{
    return stringf("[%s]", pkg_name.c_str());
}
string path_for_log(string_par path)
{
    string r = path.str();
    if (r.empty() || r.find(' ') != string::npos) {
        r.insert(0, 1, '"');
        r.push_back('"');
    }
    return r;
}
}
