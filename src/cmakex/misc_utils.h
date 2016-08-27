#ifndef MISC_UTIL_23923948
#define MISC_UTIL_23923948

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <vector>

#include <adasworks/sx/check.h>
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

bool ends_with(string_par x, string_par y);

// return x but the leftmost z characters. If x contains less then z characters, return empty
array_view<const char> butleft(string_par x, int z);

// array_view to string
inline string make_string(array_view<const char> x)
{
    return string(x.begin(), x.end());
}

template <class T>
std::vector<T> to_vector(const std::set<T>& x)
{
    std::vector<T> y;
    y.reserve(x.size());
    for (auto& i : x)
        y.emplace_back(i);
    return y;
}

// join the items of v with separator string s
string join(const vector<string>& v, const string& s);
inline string join(const std::set<string>& v, const string& s)
{
    return join(to_vector(v), s);
}

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
inline int ferror(const file_t& f)
{
    return ::ferror(f.stream());
}
inline size_t fread(void* buffer, size_t size, size_t count, const file_t& f)
{
    return ::fread(buffer, size, count, f.stream());
}

// expects the file was opened in "r" mode which is text mode on windows
// reads f until eol or eof
// fails on error
// does not include eol
// returns empty if it was eof before calling
string must_fgetline_if_not_eof(const file_t& f);

inline bool feof(const file_t& f)
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

vector<string> must_read_file_as_lines(string_par filename);
vector<string> split(string_par x, char y);

template <class C>
C set_difference(const C& x, const C& y)
{
    CHECK(std::is_sorted(BEGINEND(x)));
    CHECK(std::is_sorted(BEGINEND(y)));
    C r(std::max(x.size(), y.size()));
    r.erase(std::set_difference(BEGINEND(x), BEGINEND(y), r.begin()), r.end());
    return r;
}

string file_sha(string_par path);
string string_sha(const string& x);

template <class Container1, class Container2>
void append_inplace(Container1& c1, const Container2& c2)
{
    c1.insert(c1.end(), c2.begin(), c2.end());
}

template <class T>
void reserve_if_applicable(T&, typename T::size_type)
{
}

template <class X>
void reserve_if_applicable(std::vector<X>& x, typename std::vector<X>::size_type y)
{
    x.reserve(y);
}

template <class X>
void reserve_if_applicable(std::deque<X>& x, typename std::deque<X>::size_type y)
{
    x.reserve(y);
}

template <class Container1, class Container2>
Container1 concat(const Container1& c1, const Container2& c2)
{
    Container1 r;
    reserve_if_applicable(c1, c1.size() + c2.size());
    r = c1;
    append_inplace(r, c2);
    return r;
}

template <class Container1, class Container2>
void prepend_inplace(Container1& c1, const Container2& c2)
{
    c1.insert(c1.begin(), c2.begin(), c2.end());
}

template <class Map>
std::vector<typename Map::key_type> keys_of_map(const Map& m)
{
    std::vector<typename Map::key_type> v;
    v.reserve(m.size());
    for (auto& kv : m)
        v.emplace_back(kv.first);
    return v;
}

bool tolower_equals(string_par x, string_par y);

// remote whitespace before and after x
string trim(string_par x);
string tolower(string_par x);

// like std::binary_search but linear
template <class Cont, class Elem>
bool linear_search(const Cont& c, const Elem& e)
{
    return std::find(BEGINEND(c), e) != c.end();
}

// cheap n^2 alg
template <class T>
vector<T> stable_unique(const vector<T>& x)
{
    vector<T> y;
    y.reserve(x.size());
    for (auto& xe : x) {
        if (!linear_search(y, xe))
            y.emplace_back(xe);
    }
    return y;
}
}

#endif
