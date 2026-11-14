#include <internal/platform.h>
#include <internal/win32con.h>
#include <internal/winwidth.h>
#include <internal/conctl.h>

#include <locale.h>
#include <wctype.h>
#include <wchar.h>

namespace tvision
{

// Эти функции непосредственно использует TText. Они хранятся в отдельном
// глобальном объекте и доступны независимо от инициализации Platform.
// Реализация находится в отдельной единице трансляции, чтобы приложение
// могло связаться с ней без всего класса Platform и его зависимостей,
// например в консольной утилите с TText.
//
//

Platform::CharOps Platform::charOps =
{
    &initCharWidth,
    &initCharToLower,
};

void Platform::initLocale() noexcept
{
    static int init = [] ()
    {
        // Используем преобразование символов из среды выполнения C, зависящее от
        // локали C.
#ifdef _WIN32
        // В Windows явно задаём локаль .utf8, также включающую поддержку UTF-8 в
        // файловых функциях. Это делает и Win32ConsoleAdapter. См.
        // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale#utf-8-support
        //
        setlocale(LC_ALL, ".utf8");
#else
        setlocale(LC_ALL, "");
#endif // _WIN32
        (void) init;
        return 0;
    }();
}

#ifdef _WIN32
static int windowsCharWidth(uint32_t wc) noexcept
{
    return WinWidth::width(wc);
}
#else
static int unixCharWidth(uint32_t wc) noexcept
{
    return wcwidth(wc);
}
#   ifdef __linux__
static int linuxConsoleCharWidth(uint32_t wc) noexcept
{
    // Консоль Linux не поддерживает символы нулевой ширины: все считаются
    // одинарными либо двойными. Набор широких символов совпадает с
    // реализацией wcwidth() Маркуса Куна от 2007-05-26:
    // https://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
    return 1 +
        (wc >= 0x1100 &&
         (wc <= 0x115f ||
          wc == 0x2329 || wc == 0x232a ||
          (wc >= 0x2e80 && wc <= 0xa4cf &&
           wc != 0x303f) ||
          (wc >= 0xac00 && wc <= 0xd7a3) ||
          (wc >= 0xf900 && wc <= 0xfaff) ||
          (wc >= 0xfe10 && wc <= 0xfe19) ||
          (wc >= 0xfe30 && wc <= 0xfe6f) ||
          (wc >= 0xff00 && wc <= 0xff60) ||
          (wc >= 0xffe0 && wc <= 0xffe6) ||
          (wc >= 0x20000 && wc <= 0x2fffd) ||
          (wc >= 0x30000 && wc <= 0x3fffd)));
}
#   endif // __linux__
#endif // _WIN32

static uint32_t charToLower(uint32_t ch) noexcept
{
    // Пропускаем преобразование, если символ не помещается в один wchar_t.
    if (sizeof(uint32_t) > sizeof(wchar_t) && ch > WCHAR_MAX)
        return ch;
    return (uint32_t) towlower((wchar_t) ch);
}

void Platform::initCharOps() noexcept
{
    static int init = [] ()
    {
        initLocale();

        CharOps charOps {};
#ifdef _WIN32
        charOps.width = windowsCharWidth;
#else
        charOps.width = unixCharWidth;
#   ifdef __linux__
        auto &con = ConsoleCtl::getInstance();
        if (con.isLinuxConsole())
            charOps.width = linuxConsoleCharWidth;
#   endif // __linux__
#endif // _WIN32

        charOps.toLower = charToLower;

        Platform::charOps = charOps;

        (void) init;
        return 0;
    }();
}

int Platform::initCharWidth(uint32_t wc) noexcept
{
    initCharOps();
    return charOps.width(wc);
}

uint32_t Platform::initCharToLower(uint32_t wc) noexcept
{
    initCharOps();
    return charOps.toLower(wc);
}

} // пространство имён tvision
