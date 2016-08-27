#include "filesystem.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstdlib>
#include <memory>

#include <Poco/File.h>
#include <Poco/Path.h>

#include <adasworks/sx/check.h>
#include <adasworks/sx/log.h>
#include <adasworks/sx/string_par.h>
#include <adasworks/sx/stringf.h>

#include <nowide/convert.hpp>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRALEAN
#include <Windows.h>
//#include <WinIoCtl.h>
#else
#include <unistd.h>
#endif

namespace filesystem {

using adasworks::sx::stringf;
using adasworks::sx::string_par;
using std::string;
using std::exception;

namespace {
#ifdef _WIN32
string message_from_windows_system_error_code(DWORD code)
{
    wchar_t* msgBuf = nullptr;
    auto r = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgBuf, 0, NULL);

    if (r == 0)  // FormatMessage failed
        return stringf(
            "(can't to obtain system error message, FormatMessage failed with error code %u)",
            (unsigned)GetLastError());

    // success
    try {
        string msg_buf = nowide::narrow(msgBuf);
        LocalFree(msgBuf);
        return msg_buf;
    } catch (const exception& e) {
        LocalFree(msgBuf);
        return stringf("(can't obtain system error message, nowide::narrow failed with '%s')",
                       e.what());
    } catch (...) {
        LocalFree(msgBuf);
        return stringf(
            "(can't obtain system error message, nowide::narrow failed with unknown "
            "exception)");
    }
}

#if defined _MSC_VER && _MSC_VER < 1900
__declspec(noreturn)
#else
[[noreturn]]
#endif
    void throw_filesystem_error_from_windows_system_error_code(DWORD code,
                                                               string_par base_msg,
                                                               const path* p1 = nullptr,
                                                               const path* p2 = nullptr)
{
    auto error_msg = message_from_windows_system_error_code(code);
    auto msg = stringf("%s, reason: %s", base_msg.c_str(), error_msg.c_str());
    auto ec = std::error_code(code, std::system_category());
    if (!p1) {
        CHECK(!p2);
        throw filesystem_error(msg, ec);
    } else if (!p2)
        throw filesystem_error(msg, *p1, ec);
    else
        throw filesystem_error(msg, *p1, *p2, ec);
}
#endif

#if defined _MSC_VER && _MSC_VER < 1900
__declspec(noreturn)
#else
[[noreturn]]
#endif
    void throw_filesystem_error_from_errno_code(int code,
                                                string_par base_msg,
                                                const path* p1 = nullptr,
                                                const path* p2 = nullptr)
{
    auto msg = stringf("%s, reason: %s", base_msg.c_str(), strerror(errno));
    auto ec = std::error_code(code, std::system_category());
    if (!p1) {
        CHECK(!p2);
        throw filesystem_error(msg, ec);
    } else if (!p2)
        throw filesystem_error(msg, *p1, ec);
    else
        throw filesystem_error(msg, *p1, *p2, ec);
}
}

path current_path()
{
#ifdef _WIN32
    auto buf = _wgetcwd(nullptr, 0);
    if (buf) {
        try {
            path result(nowide::narrow(buf));
#else
    auto buf = getcwd(nullptr, 0);
    if (buf) {
        try {
            path result(buf);
#endif
            free(buf);
            return result;
        } catch (...) {
            free(buf);
            throw;
        }
    } else
        throw_filesystem_error_from_errno_code(errno, "Can't get current working directory");
}

void current_path(const path& p)
{
#ifdef _WIN32
    if (SetCurrentDirectoryW(nowide::widen(p.c_str()).c_str()))
        return;
    throw_filesystem_error_from_windows_system_error_code(
        GetLastError(), stringf("Can't change current directory to %s", p.c_str()).c_str(), &p);
#else
    if (::chdir(p.c_str()) == 0)
        return;
    throw_filesystem_error_from_errno_code(
        errno, stringf("Can't change current directory to %s", p.c_str()), &p);
#endif
}

namespace {
#ifdef _WIN32
bool not_found_error(int errval)
{
    return errval == ERROR_FILE_NOT_FOUND || errval == ERROR_PATH_NOT_FOUND ||
           errval == ERROR_INVALID_NAME          // "tools/jam/src/:sys:stat.h", "//foo"
           || errval == ERROR_INVALID_DRIVE      // USB card reader with no card inserted
           || errval == ERROR_NOT_READY          // CD/DVD drive with no disc inserted
           || errval == ERROR_INVALID_PARAMETER  // ":sys:stat.h"
           || errval == ERROR_BAD_PATHNAME       // "//nosuch" on Win64
           || errval == ERROR_BAD_NETPATH;       // "//nosuch" on Win32
}
file_status process_status_failure(const path& p, std::error_code* ec)
{
    int errval(::GetLastError());
    if (ec != 0)                                     // always report errval, even though some
        ec->assign(errval, std::system_category());  // errval values are not status_errors

    if (not_found_error(errval)) {
        return file_status(file_type::not_found, perms::none);
    } else if ((errval == ERROR_SHARING_VIOLATION)) {
        return file_status(file_type::unknown);
    }
    if (ec == 0)
        throw_filesystem_error_from_windows_system_error_code(
            errval, stringf("Can't get file status for %s", p.c_str()), &p);
    return file_status(file_type::none);
}
perms make_permissions(const path& p, DWORD attr)
{
    perms prms = (perms)((int)perms::owner_read | (int)perms::group_read | (int)perms::others_read);
    if ((attr & FILE_ATTRIBUTE_READONLY) == 0)
        prms = (perms)((int)prms | (int)perms::owner_write | (int)perms::group_write |
                       (int)perms::others_write);
    if (_stricmp(p.extension().string().c_str(), ".exe") == 0 ||
        _stricmp(p.extension().string().c_str(), ".com") == 0 ||
        _stricmp(p.extension().string().c_str(), ".bat") == 0 ||
        _stricmp(p.extension().string().c_str(), ".cmd") == 0)
        prms = (perms)((int)prms | (int)perms::owner_exec | (int)perms::group_exec |
                       (int)perms::others_exec);
    return prms;
}
struct handle_wrapper
{
    HANDLE handle;
    handle_wrapper(HANDLE h) : handle(h) {}
    ~handle_wrapper()
    {
        if (handle != INVALID_HANDLE_VALUE)
            ::CloseHandle(handle);
    }
};
HANDLE create_file_handle(const path& p,
                          DWORD dwDesiredAccess,
                          DWORD dwShareMode,
                          LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                          DWORD dwCreationDisposition,
                          DWORD dwFlagsAndAttributes,
                          HANDLE hTemplateFile)
{
    return ::CreateFileW(nowide::widen(p).c_str(), dwDesiredAccess, dwShareMode,
                         lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
                         hTemplateFile);
}

//  REPARSE_DATA_BUFFER related definitions are found in ntifs.h, which is part of the
//  Windows Device Driver Kit. Since that's inconvenient, the definitions are provided
//  here. See http://msdn.microsoft.com/en-us/library/ms791514.aspx

#if !defined(REPARSE_DATA_BUFFER_HEADER_SIZE)  // mingw winnt.h does provide the defs

#define SYMLINK_FLAG_RELATIVE 1

typedef struct _REPARSE_DATA_BUFFER
{
    ULONG ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union
    {
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
            /*  Example of distinction between substitute and print names:
    mklink /d ldrive c:\
    SubstituteName: c:\\??\
    PrintName: c:\
    */
        } SymbolicLinkReparseBuffer;
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct
        {
            UCHAR DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)

#endif

#ifndef MAXIMUM_REPARSE_DATA_BUFFER_SIZE
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE (16 * 1024)
#endif

#ifndef FSCTL_GET_REPARSE_POINT
#define FSCTL_GET_REPARSE_POINT 0x900a8
#endif

#ifndef IO_REPARSE_TAG_SYMLINK
#define IO_REPARSE_TAG_SYMLINK (0xA000000CL)
#endif

bool is_reparse_point_a_symlink(const path& p)
{
    handle_wrapper h(create_file_handle(
        p, FILE_READ_EA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL));
    if (h.handle == INVALID_HANDLE_VALUE)
        return false;

    std::unique_ptr<char[]> buf(new char[MAXIMUM_REPARSE_DATA_BUFFER_SIZE]);

    // Query the reparse data
    DWORD dwRetLen;
    BOOL result = ::DeviceIoControl(h.handle, FSCTL_GET_REPARSE_POINT, NULL, 0, buf.get(),
                                    MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRetLen, NULL);
    if (!result)
        return false;

    return reinterpret_cast<const REPARSE_DATA_BUFFER*>(buf.get())->ReparseTag ==
               IO_REPARSE_TAG_SYMLINK
           // Issue 9016 asked that NTFS directory junctions be recognized as directories.
           // That is equivalent to recognizing them as symlinks, and then the normal symlink
           // mechanism will take care of recognizing them as directories.
           //
           // Directory junctions are very similar to symlinks, but have some performance
           // and other advantages over symlinks. They can be created from the command line
           // with "mklink /j junction-name target-path".
           ||
           reinterpret_cast<const REPARSE_DATA_BUFFER*>(buf.get())->ReparseTag ==
               IO_REPARSE_TAG_MOUNT_POINT;  // aka "directory junction" or "junction"
}
#else
bool not_found_error(int /*errval*/)
{
    return errno == ENOENT || errno == ENOTDIR;
}
#endif
}

file_status status(const path& p, std::error_code* ec)
{
#ifndef _WIN32

    struct stat path_stat;
    if (::stat(p.c_str(), &path_stat) != 0) {
        if (ec != 0)                                    // always report errno, even though some
            ec->assign(errno, std::system_category());  // errno values are not status_errors

        if (not_found_error(errno))
            return file_status(file_type::not_found, perms::none);
        if (errno == EACCES)
            return file_status(file_type::unknown, perms::none);
        if (ec == 0)
            throw_filesystem_error_from_errno_code(
                errno, stringf("Can't get file status for %s", p.c_str()), &p);
        return file_status(file_type::none);
    }
    if (ec)
        ec->clear();
    if (S_ISREG(path_stat.st_mode))
        return file_status(file_type::regular,
                           static_cast<perms>(path_stat.st_mode & (int)perms::mask));
    if (S_ISDIR(path_stat.st_mode))
        return file_status(file_type::directory,
                           static_cast<perms>(path_stat.st_mode & (int)perms::mask));
    if (S_ISBLK(path_stat.st_mode))
        return file_status(file_type::block,
                           static_cast<perms>(path_stat.st_mode & (int)perms::mask));
    if (S_ISCHR(path_stat.st_mode))
        return file_status(file_type::character,
                           static_cast<perms>(path_stat.st_mode & (int)perms::mask));
    if (S_ISFIFO(path_stat.st_mode))
        return file_status(file_type::fifo,
                           static_cast<perms>(path_stat.st_mode & (int)perms::mask));
    if (S_ISSOCK(path_stat.st_mode))
        return file_status(file_type::socket,
                           static_cast<perms>(path_stat.st_mode & (int)perms::mask));
    return file_status(file_type::unknown);

#else  // Windows

    DWORD attr(::GetFileAttributesW(nowide::widen(p).c_str()));
    if (attr == 0xFFFFFFFF) {
        return process_status_failure(p, ec);
    }

    //  reparse point handling;
    //    since GetFileAttributesW does not resolve symlinks, try to open a file
    //    handle to discover if the file exists
    if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
        handle_wrapper h(create_file_handle(p,
                                            0,  // dwDesiredAccess; attributes only
                                            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            0,  // lpSecurityAttributes
                                            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS,
                                            0));  // hTemplateFile
        if (h.handle == INVALID_HANDLE_VALUE) {
            return process_status_failure(p, ec);
        }

        if (!is_reparse_point_a_symlink(p))
            return file_status(file_type::symlink, make_permissions(p, attr));
    }

    if (ec != 0)
        ec->clear();
    return (attr & FILE_ATTRIBUTE_DIRECTORY)
               ? file_status(file_type::directory, make_permissions(p, attr))
               : file_status(file_type::regular, make_permissions(p, attr));

#endif
}

file_status status(const path& p)
{
    return status(p, nullptr);
}
file_status status(const path& p, std::error_code& ec)
{
    return status(p, &ec);
}

namespace {
#ifdef _WIN32

const char separator = '/';
const char* const separators = "/\\";
const char* separator_string = "/";
const char* preferred_separator_string = "\\";
const char colon = ':';
const char dot = '.';
const char questionmark = '?';

inline bool is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

#else

const char separator = '/';
const char* const separators = "/";
// const char* separator_string = "/";
// const char* preferred_separator_string = "/";
// const char dot = '.';

#endif

inline bool is_separator(path::value_type c)
{
    return c == separator
#ifdef _WIN32
           || c == path::preferred_separator
#endif
        ;
}

size_t filename_pos(const path::string_type& str,
                    size_t end_pos)  // end_pos is past-the-end position
// return 0 if str itself is filename (or empty)
{
    // case: "//"
    if (end_pos == 2 && is_separator(str[0]) && is_separator(str[1]))
        return 0;

    // case: ends in "/"
    if (end_pos && is_separator(str[end_pos - 1]))
        return end_pos - 1;

    // set pos to start of last element
    size_t pos(str.find_last_of(separators, end_pos - 1));

#ifdef _WIN32
    if (pos == path::string_type::npos && end_pos > 1)
        pos = str.find_last_of(colon, end_pos - 2);
#endif

    return (pos == path::string_type::npos          // path itself must be a filename (or empty)
            || (pos == 1 && is_separator(str[0])))  // or net
               ? 0                                  // so filename is entire string
               : pos + 1;                           // or starts after delimiter
}

bool is_root_separator(const path::string_type& str, size_t pos)
// pos is position of the separator
{
    CHECK(!str.empty() && is_separator(str[pos]));

    // subsequent logic expects pos to be for leftmost slash of a set
    while (pos > 0 && is_separator(str[pos - 1]))
        --pos;

    //  "/" [...]
    if (pos == 0)
        return true;

#ifdef _WIN32
    //  "c:/" [...]
    if (pos == 2 && is_letter(str[0]) && str[1] == colon)
        return true;
#endif

    //  "//" name "/"
    if (pos < 3 || !is_separator(str[0]) || !is_separator(str[1]))
        return false;

    return str.find_first_of(separators, 2) == pos;
}
}  // namepace

path path::filename() const
{
    auto pos = filename_pos(s, s.size());
    return (s.size() && pos && is_separator(s[pos]) && !is_root_separator(s, pos))
               ? path(".")
               : path(s.c_str() + pos);
}

path path::extension() const
{
    path name(filename());
    if (name.s == "." || name.s == "..")
        return path();
    auto pos = name.s.rfind('.');
    return pos == string_type::npos ? path() : path(name.s.c_str() + pos);
}

bool path::is_absolute() const
{
    return Poco::Path(s).isAbsolute();
}

bool path::is_relative() const
{
    return Poco::Path(s).isRelative();
}

path path::parent_path() const
{
    return Poco::Path(s).parent().toString();
}

path path::stem() const
{
    return Poco::Path(s).getBaseName();
}

void remove(const path& p)
{
    Poco::File(p.string()).remove();
}
void remove_all(const path& p)
{
    Poco::File(p.string()).remove(true);
}
void create_directories(const path& p)
{
    Poco::File(p.string()).createDirectories();
}
bool exists(const path& p)
{
    return Poco::File(p.string()).exists();
}
path temp_directory_path()
{
    return Poco::Path::temp();
}
path canonical(const path& p, const path& base)
{
    // todo resolve symlinks
    return lexically_normal(absolute(p, base));
}

path absolute(const path& p, const path& base)
{
    return Poco::Path(p.string()).absolute(Poco::Path(base.string())).toString();
}

path lexically_normal(const path& p)
{
    auto ps = p.string();
    if (ps.empty())
        return {};
    if (ps == "/" || ps == "\\")
        return Poco::Path(true).toString();

    auto pp = Poco::Path(p.string());
    std::vector<std::string> comps;
    bool trailing_dot = false;

    for (int i = 0; i <= pp.depth(); ++i) {
        auto& c = pp[i];
        if (c == "." || c.empty()) {
            if (i == pp.depth())
                trailing_dot = true;
            continue;
        }
        if (c == ".." && !comps.empty())
            comps.pop_back();
        else
            comps.emplace_back(c);
    }
    if (trailing_dot)
        comps.emplace_back(".");

    Poco::Path pq(pp.isAbsolute());
    pq.setNode(pp.getNode());
    pq.setDevice(pp.getDevice());

    for (int i = 0; i + 1 < comps.size(); ++i)
        pq.pushDirectory(comps[i]);
    if (!comps.empty())
        pq.setFileName(comps.back());

    return pq.toString();
}

#ifdef _WIN32
struct handle_wrapper
{
    HANDLE handle;
    handle_wrapper(HANDLE h) : handle(h) {}
    ~handle_wrapper()
    {
        if (handle != INVALID_HANDLE_VALUE)
            ::CloseHandle(handle);
    }
};
HANDLE create_file_handle(const path& p,
                          DWORD dwDesiredAccess,
                          DWORD dwShareMode,
                          LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                          DWORD dwCreationDisposition,
                          DWORD dwFlagsAndAttributes,
                          HANDLE hTemplateFile)
{
    return ::CreateFileW(p.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                         dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}
#endif

bool equivalent(const path& p1, const path& p2)
{
#ifndef _WIN32
    struct stat s2;
    int e2(::stat(p2.c_str(), &s2));
    struct stat s1;
    int e1(::stat(p1.c_str(), &s1));

    if (e1 != 0 || e2 != 0) {
        // if one is invalid and the other isn't then they aren't equivalent,
        // but if both are invalid then it is an error
        auto e = e1 != 0 && e2 != 0;
        if (e)
            throw_filesystem_error_from_errno_code(e, "Both paths are invalid", &p1, &p2);
        return false;
    }

    // both stats now known to be valid
    return s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino
           // According to the POSIX stat specs, "The st_ino and st_dev fields
           // taken together uniquely identify the file within the system."
           // Just to be sure, size and mod time are also checked.
           && s1.st_size == s2.st_size && s1.st_mtime == s2.st_mtime;

#else  // Windows

    // Note well: Physical location on external media is part of the
    // equivalence criteria. If there are no open handles, physical location
    // can change due to defragmentation or other relocations. Thus handles
    // must be held open until location information for both paths has
    // been retrieved.

    // p2 is done first, so any error reported is for p1
    handle_wrapper h2(create_file_handle(p2.c_str(), 0,
                                         FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0));

    handle_wrapper h1(create_file_handle(p1.c_str(), 0,
                                         FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0));

    if (h1.handle == INVALID_HANDLE_VALUE || h2.handle == INVALID_HANDLE_VALUE) {
        // if one is invalid and the other isn't, then they aren't equivalent,
        // but if both are invalid then it is an error
        auto e = (h1.handle == INVALID_HANDLE_VALUE && h2.handle == INVALID_HANDLE_VALUE)
                     ? ERROR_NOT_SUPPORTED
                     : 0;
        if (e)
            throw_filesystem_error_from_windows_system_error_code(e, "Both paths are invalid", &p1,
                                                                  &p2);
        return false;
    }

    // at this point, both handles are known to be valid

    BY_HANDLE_FILE_INFORMATION info1, info2;

    auto e1 = !::GetFileInformationByHandle(h1.handle, &info1) ? ::GetLastError() : 0;
    if (e1)
        throw_filesystem_error_from_windows_system_error_code(
            e1, "GetFileInformationByHandle() failed", &p1);

    auto e2 = !::GetFileInformationByHandle(h2.handle, &info2) ? ::GetLastError() : 0;
    if (e2)
        throw_filesystem_error_from_windows_system_error_code(
            e2, "GetFileInformationByHandle() failed", &p2);

    // In theory, volume serial numbers are sufficient to distinguish between
    // devices, but in practice VSN's are sometimes duplicated, so last write
    // time and file size are also checked.
    return info1.dwVolumeSerialNumber == info2.dwVolumeSerialNumber &&
           info1.nFileIndexHigh == info2.nFileIndexHigh &&
           info1.nFileIndexLow == info2.nFileIndexLow &&
           info1.nFileSizeHigh == info2.nFileSizeHigh && info1.nFileSizeLow == info2.nFileSizeLow &&
           info1.ftLastWriteTime.dwLowDateTime == info2.ftLastWriteTime.dwLowDateTime &&
           info1.ftLastWriteTime.dwHighDateTime == info2.ftLastWriteTime.dwHighDateTime;

#endif
}
}
