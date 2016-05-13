#ifndef FILESYSTEM_23978479283
#define FILESYSTEM_23978479283

#include <adasworks/sx/array_view.h>
#include <string>

namespace filesystem {

using adasworks::sx::array_view;

class path
{
public:
#ifdef _WIN32
    using value_type = wchar_t;
#else
    using value_type = char;
#endif
    using string_type = std::basic_string<value_type>;

    path() = default;
    path(const std::string& s);  // utf8
    const value_type* c_str() const { return s.c_str(); }
    const string_type& native() const { return s; }
    operator string_type() const { return s; }
    std::string u8string() const;

private:
    string_type s;
};

enum class file_type
{
    none = 0,
    not_found = -1,
    regular = 1,
    directory = 2,
    symlink = 3,
    block = 4,
    character = 5,
    fifo = 6,
    socket = 7,
    unknown = 8
};
enum class perms
{
    none = 0,
    unknown = 0xffff
};

class file_status
{
public:
    file_status(const file_status&) = default;
    file_status(file_status&&) = default;
    explicit file_status(file_type type = file_type::none, perms permissions = perms::unknown)
        : t(type), p(permissions)
    {
    }
    file_type type() const { return t; }
    void type(file_type type) { t = type; }
private:
    file_type t;
    perms p;
};

inline bool is_regular_file(file_status s)
{
    return s.type() == file_type::regular;
}
bool is_regular_file(const path& p);
path current_path();
}

#endif
