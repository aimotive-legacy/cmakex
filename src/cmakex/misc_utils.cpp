#include "misc_utils.h"

#include <cerrno>

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
        throwf_errno("Can't open \"%s\" in mode \"%s\"", path.c_str(), mode.c_str());
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
        throwf_errno("Write error in file \"%s\"", f.path().c_str());
}
int fprintf(const file_t& f, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    int r = vfprintf(f.stream(), format, ap);
    va_end(ap);
    return r;
}
string strip_trailing_whitespace(string_par x)
{
    string s = x.str();
    while (!x.empty() && iswspace(s.back()))
        s.pop_back();
    return s;
}
}
