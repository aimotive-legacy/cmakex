#include "misc_util.h"

namespace cmakex {
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
}
