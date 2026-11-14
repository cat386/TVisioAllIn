#ifndef __BORLANDC__
#include <internal/filesave.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace tvision
{
#ifdef _WIN32
static std::wstring widePath(const char *path)
{
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
    if (!size)
        return {};
    std::vector<wchar_t> out((size_t) size);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, out.data(), size))
        return {};
    return out.data();
}
#endif

bool saveFileAtomic(const char *path, const char *backup,
                    const char *first, size_t firstSize,
                    const char *second, size_t secondSize) noexcept
{
#ifdef _WIN32
    std::wstring target = widePath(path), backupPath;
    if (target.empty())
        return false;
    if (backup)
    {
        backupPath = widePath(backup);
        if (backupPath.empty())
            return false;
        if (_wcsicmp(target.c_str(), backupPath.c_str()) == 0)
            backupPath += L".bak";
    }
    std::wstring pattern = target + L".tvXXXXXX";
    std::vector<wchar_t> temp(pattern.begin(), pattern.end());
    temp.push_back(0);
    if (_wmktemp_s(temp.data(), temp.size()) != 0)
        return false;
    int fd = _wopen(temp.data(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd < 0)
        return false;
    FILE *file = _wfdopen(fd, L"wb");
#else
    // Keep an existing symlink intact: replace the file it refers to.
    char *resolved = realpath(path, nullptr);
    std::string target = resolved ? resolved : path;
    free(resolved);
    std::string backupPath = backup ? backup : "";
    if (backupPath == target)
        backupPath += ".bak";
    std::string pattern = target + ".tvXXXXXX";
    std::vector<char> temp(pattern.begin(), pattern.end());
    temp.push_back(0);
    int fd = mkstemp(temp.data());
    if (fd < 0)
        return false;
    FILE *file = fdopen(fd, "wb");
#endif
    bool ok = false;
    if (file)
    {
        ok = (firstSize == 0 || fwrite(first, 1, firstSize, file) == firstSize)
          && (secondSize == 0 || fwrite(second, 1, secondSize, file) == secondSize)
          && fflush(file) == 0;
#ifndef _WIN32
        struct stat original;
        if (ok && stat(target.c_str(), &original) == 0)
            ok = fchmod(fd, original.st_mode & 0777) == 0;
#endif
        if (ok)
#ifdef _WIN32
            ok = _commit(fd) == 0;
#else
            ok = fsync(fd) == 0;
#endif
        if (fclose(file) != 0)
            ok = false;
    }
    else
#ifdef _WIN32
        _close(fd);
#else
        close(fd);
#endif

    if (ok)
    {
#ifdef _WIN32
        DWORD attrs = GetFileAttributesW(target.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES)
            ok = ReplaceFileW(target.c_str(), temp.data(),
                              backup ? backupPath.c_str() : nullptr, 0, nullptr, nullptr) != 0;
        else if (GetLastError() == ERROR_FILE_NOT_FOUND)
            ok = MoveFileExW(temp.data(), target.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
        else
            ok = false;
#else
        if (backup && access(target.c_str(), F_OK) == 0)
        {
            // Install the backup atomically. An error never truncates the source.
            std::string bpattern = backupPath + ".tvXXXXXX";
            std::vector<char> btemp(bpattern.begin(), bpattern.end());
            btemp.push_back(0);
            int bfd = mkstemp(btemp.data());
            ok = bfd >= 0;
            if (ok)
            {
                close(bfd);
                unlink(btemp.data());
                ok = link(target.c_str(), btemp.data()) == 0;
                if (ok)
                    ok = rename(btemp.data(), backupPath.c_str()) == 0;
                unlink(btemp.data());
            }
        }
        if (ok)
            ok = rename(temp.data(), target.c_str()) == 0;
#endif
    }
#ifdef _WIN32
    _wunlink(temp.data());
#else
    unlink(temp.data());
#endif
    return ok;
}
}
#endif
