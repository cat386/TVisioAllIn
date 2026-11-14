#ifndef TVISION_CONCTL_H
#define TVISION_CONCTL_H

#include <tvision/tv.h>
#include <stdio.h>

#ifdef _WIN32
#include <tvision/compat/windows/windows.h>
#endif

namespace tvision
{

class ConsoleCtl
{
#ifdef _WIN32
    enum { input = 0, startupOutput = 1, activeOutput = 2 };
    struct
    {
        HANDLE handle {INVALID_HANDLE_VALUE};
        bool owning {false};
    } cn[3];
    bool ownsConsole {false};
#else
    int fds[2] {-1, -1};
    FILE *files[2] {nullptr, nullptr};
    bool ownsFiles {false};
#endif // _WIN32

    static ConsoleCtl *instance;

    ConsoleCtl() noexcept;
    ~ConsoleCtl();

public:

    // В Windows экземпляр ConsoleCtl создаётся при каждом включении
    // альтернативного экранного буфера и уничтожается при восстановлении
    // консоли. В Unix ConsoleCtl создаётся только один раз.

    // Создаёт глобальный экземпляр, если его ещё нет, и возвращает его.
    static ConsoleCtl &getInstance() noexcept;
    // Уничтожает глобальный экземпляр, если он существует.
    static void destroyInstance() noexcept;

    void write(const char *data, size_t bytes) const noexcept;
    TPoint getSize() const noexcept;
    TPoint getFontSize() const noexcept;

#ifdef _WIN32
    HANDLE in() const noexcept { return cn[input].handle; }
    HANDLE out() const noexcept { return cn[activeOutput].handle; }
#else
    int in() const noexcept { return fds[0]; }
    int out() const noexcept { return fds[1]; }
    FILE *fin() const noexcept { return files[0]; }
    FILE *fout() const noexcept { return files[1]; }
#ifdef __linux__
    bool isLinuxConsole() const noexcept;
#endif
#endif // _WIN32
};

} // пространство имён tvision

#endif // TVISION_CONCTL_H
