#ifndef MISC_UTIL_23923948
#define MISC_UTIL_23923948

#include <map>

#include <adasworks/sx/config.h>
#include <adasworks/sx/log.h>

#include "using-decls.h"

#define BEGINEND(X) (X).begin(), (X).end()
#define STRINGIZE_CORE(x) #x
#define STRINGIZE(x) STRINGIZE_CORE(x)

namespace cmakex {

// true if one of the items in y equals to x
bool is_one_of(string_par x, initializer_list<const char*> y);

// x starts with y
bool starts_with(string_par x, string_par y);

// x starts with y, case-independent
bool istarts_with(string_par x, string_par y);

// x starts with y
bool starts_with(string_par x, char y);

// return x but the leftmost z characters. If x contains less then z characters, return empty
array_view<const char> butleft(string_par x, int z);

// array_view to string
inline string make_string(array_view<const char> x)
{
    return string(x.begin(), x.end());
}

// join the items of v with separator string s
string join(const vector<string>& v, const string& s);

struct file_t
{
    explicit file_t(FILE* f) : f(f) {}
    file_t(const file_t&) = delete;
    file_t(file_t&& x) : path_(move(x.path_)), f(x.f) { x.f = nullptr; }
    file_t& operator=(const file_t&) = delete;
    file_t& operator=(file_t&& x)
    {
        file_t tmp(move(x));
        swap(tmp);
        return *this;
    }
    void swap(file_t& x)
    {
        using std::swap;
        swap(path_, x.path_);
        swap(f, x.f);
    }
    ~file_t()
    {
        if (f) {
            int r = fclose(f);
            if (r)
                LOG_DEBUG("Failed to close \"%s\"", path_.c_str());
        }
    }
    FILE* stream() const { return f; }
    const string& path() const { return path_; }
private:
    string path_;
    FILE* f = nullptr;
};

inline void swap(file_t& x, file_t& y)
{
    x.swap(y);
}

file_t must_fopen(string_par path, string_par mode);
maybe<file_t> try_fopen(string_par path, string_par mode);
void must_fprintf(const file_t& f, const char* format, ...) AW_PRINTFLIKE(2, 3);
int fprintf(const file_t& f, const char* format, ...) AW_PRINTFLIKE(2, 3);
// expects the file was opened in "r" mode which is text mode on windows
string must_fgetline_if_not_eof(const file_t& f);
bool feof(const file_t& f)
{
    return feof(f.stream()) != 0;
}

// throws std::runtime_error with formatted message
AW_NORETURN void throwf(const char* format, ...) AW_PRINTFLIKE(1, 2);

// throws std::runtime_error with formatted message, errno message appended if nonzero
AW_NORETURN void throwf_errno(const char* format, ...) AW_PRINTFLIKE(1, 2);

string strip_trailing_whitespace(string_par x);

// like the cmake function
vector<string> separate_arguments(string_par x);

template <class T, class Container>
bool is_one_of(const T& x, const Container& c)
{
    return std::find(c.begin(), c.end(), x) != c.end();
}

// like the cmake function
std::map<string, vector<string>> parse_arguments(const vector<string>& options,
                                                 const vector<string>& onevalue_args,
                                                 const vector<string>& multivalue_args,
                                                 const vector<string>& args);
std::map<string, vector<string>> parse_arguments(const vector<string>& options,
                                                 const vector<string>& onevalue_args,
                                                 const vector<string>& multivalue_args,
                                                 string_par argstr);
}

#endif
