#ifndef WINWIDTH_H
#define WINWIDTH_H

#ifdef _WIN32

#include <tvision/tv.h>
#include <tvision/compat/windows/windows.h>
#include <atomic>
#include <unordered_map>

namespace tvision
{

class WinWidth
{
    // Аналога wcwidth нет, но API консоли поддерживает скрытые буферы.
    // Поэтому ширина символа измеряется его выводом в консольный буфер и
    // проверкой позиции курсора.

    // Для каждого потока хранится отдельное состояние: mbcwidth()
    // потокобезопасна и не требует блокировок.

    static std::atomic<size_t> lastReset;
    static std::atomic<bool> isLegacyConsole;
    static thread_local WinWidth &localInstance;

    std::unordered_map<uint32_t, short> results;
    HANDLE cnHandle {INVALID_HANDLE_VALUE};
    size_t currentReset {lastReset};

    int calcWidth(uint32_t) noexcept;
    void setUp() noexcept;
    void tearDown() noexcept;

    ~WinWidth();
    struct Destructor;

public:

    static int width(uint32_t) noexcept;
    static void reset(bool) noexcept;
};

inline int WinWidth::width(uint32_t wc) noexcept
{
    return localInstance.calcWidth(wc);
}

inline void WinWidth::reset(bool isLegacyConsole) noexcept
{
    WinWidth::isLegacyConsole = isLegacyConsole;
    ++lastReset;
}

} // пространство имён tvision

#endif // _WIN32

#endif // WINWIDTH_H
