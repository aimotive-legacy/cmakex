#ifndef FILESYSTEM_23978479283
#define FILESYSTEM_23978479283

#include <string>
#include <system_error>

namespace filesystem {

// like boost/std filesystem but assumes char*/string is utf8
class path
{
public:
    using value_type = char;
    using string_type = std::basic_string<value_type>;

    path() = default;
    path(const char* s) : s(s) {}
    path(const std::string& s) : s(s) {}
    const value_type* c_str() const { return s.c_str(); }
    const string_type& string() const { return s; }
    operator string_type() const { return s; }
    path extension() const;
    path filename() const;
    bool is_relative() const;
    bool is_absolute() const;

#ifdef _WIN32
    static const value_type preferred_separator = '\\';
#else
    static const value_type preferred_separator = '/';
#endif
private:
    string_type s;
};

class filesystem_error
{
public:
    filesystem_error(const std::string& what_arg, std::error_code ec) : what_(what_arg) {}
    filesystem_error(const std::string& what_arg, const path& p1, std::error_code ec)
        : what_(what_arg), path1_(p1)
    {
    }
    filesystem_error(const std::string& what_arg,
                     const path& p1,
                     const path& p2,
                     std::error_code ec)
        : what_(what_arg), path1_(p1), path2_(p2)
    {
    }

private:
    std::string what_;
    path path1_;
    path path2_;
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
    owner_read = 0400,
    owner_write = 0200,
    owner_exec = 0100,
    group_read = 040,
    group_write = 020,
    group_exec = 010,
    others_read = 04,
    others_write = 02,
    others_exec = 01,
    mask = 07777,
    unknown = 0xffff
};

class file_status
{
public:
    file_status(const file_status&) = default;
    explicit file_status(file_type type = file_type::none, perms permissions = perms::unknown)
        : t(type), p(permissions)
    {
    }
    file_type type() const { return t; }
    void type(file_type type) { t = type; }
    perms permissions() const { return p; }
private:
    file_type t;
    perms p;
};

file_status status(const path& p);
file_status status(const path& p, std::error_code& ec);

bool exists(const path& p);
inline bool is_regular_file(file_status s)
{
    return s.type() == file_type::regular;
}
inline bool is_regular_file(const path& p)
{
    return is_regular_file(status(p));
}
inline bool is_directory(file_status s)
{
    return s.type() == file_type::directory;
}
inline bool is_directory(const path& p)
{
    return is_directory(status(p));
}
path current_path();
void current_path(const path& p);
void remove(const path& p);
void remove_all(const path& p);
void create_directories(const path& p);
path temp_directory_path();
path canonical(const path& p, const path& base = current_path());
path absolute(const path& p, const path& base = current_path());
path lexically_normal(const path& p);
bool equivalent(const path& x, const path& y);
}

#endif
