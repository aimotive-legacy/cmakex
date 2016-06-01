#ifndef CIRCULAR_DEPENDENCY_DETECTOR_983472
#define CIRCULAR_DEPENDENCY_DETECTOR_983472

#include "using-decls.h"

namespace cmakex {

// a persistent (file-based) stack of strings with additional info stored for each item
class circular_dependency_detector
{
public:
    circular_dependency_detector(string_par binary_dir);

    void push(string_par pkg_name);
    bool contains(string_par pkg_name) const;
    vector<string> get_stack_since(string_par pkg_name) const;
    void pop(string_par pkg_name);

private:
    using stack_t = vector<string>;

    stack_t load() const;
    void save(const stack_t& x);

    const string path;
};
}
#endif
