#include "circular_dependency_detector.h"

#include <adasworks/sx/check.h>

#include "cmakex_utils.h"
#include "filesystem.h"
#include "misc_utils.h"

namespace cmakex {

namespace fs = filesystem;

circular_dependency_detector::circular_dependency_detector(string_par binary_dir)
    : path(cmakex_config_t(binary_dir).cmakex_tmp_dir() + "/dependency_stack.txt")
{
}

void circular_dependency_detector::push(string_par pkg_name)
{
    auto stack = load();
    stack.emplace_back(pkg_name.str());
    save(stack);
}

bool circular_dependency_detector::contains(string_par pkg_name) const
{
    auto stack = load();
    for (auto& x : stack)
        if (x == pkg_name)
            return true;
    return false;
}

vector<string> circular_dependency_detector::get_stack_since(string_par pkg_name) const
{
    auto stack = load();
    vector<string> r;
    CHECK(!stack.empty());
    auto it = stack.end() - 1;
    while (*it != pkg_name) {
        CHECK(it != stack.begin());
        --it;
    }
    for (; it != stack.end(); ++it)
        r.emplace_back(*it);
    return r;
}

void circular_dependency_detector::pop(string_par pkg_name)
{
    auto stack = load();
    if (stack.empty() || stack.back() != pkg_name) {
        throwf(
            "Internal error: dependency stack %s",
            (stack.empty() ? string("was empty.") : stringf("top was '%s' while popping '%s'",
                                                            stack.back().c_str(), pkg_name.c_str()))
                .c_str());
    }
    save(stack);
}

circular_dependency_detector::stack_t circular_dependency_detector::load() const
{
    if (!fs::exists(path))
        return stack_t();
    auto f = must_fopen(path, "r");
    stack_t stack;
    for (;;) {
        auto l = must_fgetline_if_not_eof(f);
        if (l.empty() && feof(f))
            break;
        stack.emplace_back(l);
    }
    return stack;
}

void circular_dependency_detector::save(const stack_t& x)
{
    auto f = must_fopen(path, "w");
    for (auto& s : x)
        must_fprintf(f, "%s\n", s.c_str());
}
}
