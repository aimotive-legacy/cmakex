#include "filesystem.h"

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRALEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <cerrno>

namespace filesystem {

path::path(const std::string& s)
#ifdef _WIN32
{
    auto size = MultiByteToWideChar(CP_UTF8, 0, x.data(), (int)x.size(), nullptr, 0);
    s.resize(size);
    MultiByteToWideChar(CP_UTF8, 0, x.data(), (int)x.size(), s.data(), size);
}
#else
    : s(s)
{
}
#endif

std::string path::u8string() const
{
#ifdef _WIN32
    std::string r;
    if (!s.empty()) {
        int size = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0, NULL, NULL);
        r.resize(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)S.size(), r.data(), size, NULL, NULL);
    }
    return r;
#else
    return s;
#endif
}

bool is_regular_file(const path& p)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesW(p.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
    struct stat sb;
    if (stat(p.c_str(), &sb))
        return false;
    return S_ISREG(sb.st_mode);
#endif
}

path current_path()
{
#ifdef _WIN32
    std::wstring temp(MAX_PATH, '\0');
    if (!_wgetcwd(&temp[0], MAX_PATH))
        throw std::runtime_error("Internal error in getcwd(): " + std::to_string(GetLastError()));
    return path(temp.c_str());
#else
    char temp[PATH_MAX];
    if (::getcwd(temp, PATH_MAX) == NULL)
        throw std::runtime_error("Internal error in getcwd(): " + std::string(strerror(errno)));
    return path(temp);
#endif
}

#if 0
//todo implement for windows
file_status status(const path& p)
{
#ifdef _WIN32
#else
    struct stat sb;
    if (stat(p.c_str(), &sb))
        return file_status(file_type::not_found);
    if (S_ISREG(sb.st_mode))
        return file_status(file_type::regular);
    if (S_ISDIR(sb.st_mode))
        return file_status(file_type::directory);
    if (S_ISBLK(sb.st_mode))
        return file_status(file_type::block);
    if (S_ISCHR(sb.st_mode))
        return file_status(file_type::character);
    if (S_ISFIFO(sb.st_mode))
        return file_status(file_type::fifo);
    if (S_ISSOCK(sb.st_mode))
        return file_status(file_type::socket);
    return file_status(file_type::unknown);
#endif
}
#endif
}
