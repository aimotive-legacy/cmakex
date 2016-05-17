#ifndef MISC_UTIL_23923948
#define MISC_UTIL_23923948

#include "using-decls.h"

namespace cmakex {

// true if one of the items in y equals to x
bool is_one_of(string_par x, initializer_list<const char*> y);

// x starts with y
bool starts_with(string_par x, string_par y);

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
}

#endif
