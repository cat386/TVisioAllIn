#define Uses_TPoint
#include <tvision/tv.h>

#include <internal/conctl.h>
#include <internal/getenv.h>

namespace tvision
{

ConsoleCtl *ConsoleCtl::instance = nullptr;

ConsoleCtl &ConsoleCtl::getInstance() noexcept
{
    if (!instance)
        instance = new ConsoleCtl;
    return *instance;
}

void ConsoleCtl::destroyInstance() noexcept
{
    delete instance;
    instance = nullptr;
}

} // пространство имён tvision

#ifdef _TV_UNIX

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#if __has_include(<linux/kd.h>)
#   include <linux/kd.h>
#endif

namespace tvision
{

ConsoleCtl::ConsoleCtl() noexcept
{
    if ( getEnv<TStringView>("TVISION_USE_STDIO").empty()
         && (files[0] = fopen("/dev/tty", "r")) != nullptr
         && (files[1] = fopen("/dev/tty", "w")) != nullptr )
    {
        ownsFiles = true;
        fds[0] = fileno(files[0]);
        fds[1] = fileno(files[1]);
        // Дочерние процессы не должны наследовать эти файловые дескрипторы.
        for (int fd : fds)
            fcntl(fd, F_SETFD, FD_CLOEXEC);
    }
    else
    {
        for (FILE *file : files)
            if (file != nullptr)
                fclose(file);
        fds[0] = STDIN_FILENO;
        fds[1] = STDOUT_FILENO;
        files[0] = stdin;
        files[1] = stdout;
    }
}

ConsoleCtl::~ConsoleCtl()
{
    if (ownsFiles)
        for (FILE *file : files)
            fclose(file);
}

void ConsoleCtl::write(const char *data, size_t bytes) const noexcept
{
    fflush(fout());
    size_t written = 0;
    int r;
    while ( written < bytes &&
            0 <= (r = ::write(out(), data + written, bytes - written)) )
        written += r;
}

TPoint ConsoleCtl::getSize() const noexcept
{
    struct winsize w;
    for (int fd : fds)
    {
        if (ioctl(fd, TIOCGWINSZ, &w) != -1)
        {
            int env_col = getEnv<int>("COLUMNS", INT_MAX);
            int env_row = getEnv<int>("LINES", INT_MAX);
            return {
                min(max(w.ws_col, 0), max(env_col, 0)),
                min(max(w.ws_row, 0), max(env_row, 0)),
            };
        }
    }
    return {0, 0};
}

TPoint ConsoleCtl::getFontSize() const noexcept
{
#ifdef KDFONTOP
    struct console_font_op cfo {};
    cfo.op = KD_FONT_OP_GET;
    cfo.width = cfo.height = 32;
    for (int fd : fds)
        if (ioctl(fd, KDFONTOP, &cfo) != -1)
            return {
                max(cfo.width, 0),
                max(cfo.height, 0),
            };
#endif
    struct winsize w;
    for (int fd : fds)
        if (ioctl(fd, TIOCGWINSZ, &w) != -1)
            return {
                w.ws_xpixel / max(w.ws_col, 1),
                w.ws_ypixel / max(w.ws_row, 1),
            };
    return {0, 0};
}

#ifdef __linux

bool ConsoleCtl::isLinuxConsole() const noexcept
{
    // Та же функция используется для получения модификаторов Shift/Ctrl/Alt
    // консоли. Работает только с файловым дескриптором консоли.
    for (int fd : fds)
    {
        char subcode = 6;
        if (ioctl(fd, TIOCLINUX, &subcode) != -1)
            return true;
    }
    return false;
}

#endif // __linux__

} // пространство имён tvision

#elif defined(_WIN32)

#include <stdio.h>

namespace tvision
{


static bool isValid(HANDLE h)
{
    return h && h != INVALID_HANDLE_VALUE;
}

static bool isConsole(HANDLE h)
{
    DWORD mode;
    return GetConsoleMode(h, &mode);
}

static COORD windowSize(SMALL_RECT srWindow)
{
    return {
        short(srWindow.Right - srWindow.Left + 1),
        short(srWindow.Bottom - srWindow.Top + 1),
    };
}

ConsoleCtl::ConsoleCtl() noexcept
{
    // Доступ к консоли возможен через GetStdHandle() и CreateFile(). При
    // перенаправленных стандартных дескрипторах GetStdHandle() не вернёт
    // консольный дескриптор. Кроме того, нужно создавать консоль, если
    // пользователь её не видит. Это бывает при аварии консоли, когда все
    // операции с её дескрипторами завершаются ошибкой, либо когда консоль
    // существует, но её невозможно показать даже через GetConsoleWindow() и
    // ShowWindow(SW_SHOW). Второй случай возникает в Git Bash без поддержки
    // псевдоконсоли: стандартные дескрипторы не относятся к консоли, но
    // полученные CreateFile() продолжают работать. Поэтому проверяем,
    // является ли консольным хотя бы один стандартный дескриптор; иначе
    // выделяем новую консоль. Это также создаст консоль при перенаправлении
    // всех трёх стандартных потоков, но такой случай редок. CreateFile() не
    // зависит от перенаправления, однако некоторые эмуляторы, например
    // ConEmu, неожиданно ведут себя с открытыми этим способом экранными
    // буферами. Поэтому по возможности используем стандартные дескрипторы.
    // Дескрипторы CreateFile() нужно закрывать, а GetStdHandle() — нельзя,
    // поэтому храним этот признак для каждого дескриптора. Также запоминаем,
    // выделили ли мы консоль, чтобы затем освободить её; иначе возможны
    // странные эффекты.
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //

    static constexpr struct { DWORD std; int index; } channels[] =
    {
        {STD_INPUT_HANDLE, input},
        {STD_OUTPUT_HANDLE, startupOutput},
        {STD_ERROR_HANDLE, startupOutput},
    };
    bool haveConsole = false;
    for (const auto &c : channels)
    {
        HANDLE h = GetStdHandle(c.std);
        if (isConsole(h))
        {
            haveConsole = true;
            if (!isValid(cn[c.index].handle))
                cn[c.index] = {h, false};
        }
    }
    if (!haveConsole)
    {
        FreeConsole();
        AllocConsole();
        ownsConsole = true;
    }
    if (!isValid(cn[input].handle))
    {
        cn[input].handle = CreateFileW(
            L"CONIN$",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            0,
            0);
        cn[input].owning = true;
    }
    if (!isValid(cn[startupOutput].handle))
    {
        cn[startupOutput].handle = CreateFileW(
            L"CONOUT$",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            0);
        cn[startupOutput].owning = true;
    }
    cn[activeOutput].handle = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        CONSOLE_TEXTMODE_BUFFER,
        nullptr);
    cn[activeOutput].owning = true;
    {
        CONSOLE_SCREEN_BUFFER_INFO sbInfo {};
        GetConsoleScreenBufferInfo(cn[startupOutput].handle, &sbInfo);
        // Приводим размер экранного буфера к размеру окна. API консоли
        // гарантирует это, но некоторые реализации, например Wine, нарушают
        // условие.
        sbInfo.dwSize = windowSize(sbInfo.srWindow);
        SetConsoleScreenBufferSize(cn[activeOutput].handle, sbInfo.dwSize);
    }
    SetConsoleActiveScreenBuffer(cn[activeOutput].handle);
    for (auto &c : cn)
        if (!isValid(c.handle))
        {
            fputs("Error: cannot get a console.\n", stderr);
            exit(1);
        }
}

ConsoleCtl::~ConsoleCtl()
{
    CONSOLE_SCREEN_BUFFER_INFO activeSbInfo {};
    GetConsoleScreenBufferInfo(cn[activeOutput].handle, &activeSbInfo);
    CONSOLE_SCREEN_BUFFER_INFO startupSbInfo {};
    GetConsoleScreenBufferInfo(cn[startupOutput].handle, &startupSbInfo);

    COORD activeWindowSize = windowSize(activeSbInfo.srWindow);
    COORD startupWindowSize = windowSize(startupSbInfo.srWindow);

    // Сохраняем текущий размер окна.
    if ( activeWindowSize.X != startupWindowSize.X ||
         activeWindowSize.Y != startupWindowSize.Y )
    {
        // Буфер не может быть меньше окна, поэтому при необходимости увеличиваем
        // его. В обратном случае не уменьшаем, чтобы не потерять данные.
        //
        COORD dwSize = startupSbInfo.dwSize;
        if (dwSize.X < activeWindowSize.X)
            dwSize.X = activeWindowSize.X;
        if (dwSize.Y < activeWindowSize.Y)
            dwSize.Y = activeWindowSize.Y;
        SetConsoleScreenBufferSize(cn[startupOutput].handle, dwSize);
        // Получаем обновлённую позицию курсора: она могла измениться после смены размера.
        GetConsoleScreenBufferInfo(cn[startupOutput].handle, &startupSbInfo);
        // Обеспечиваем видимость курсора, по возможности в нижней строке.
        SMALL_RECT srWindow = startupSbInfo.srWindow;
        COORD dwCursorPosition = startupSbInfo.dwCursorPosition;
        srWindow.Right = max(dwCursorPosition.X, activeWindowSize.X - 1);
        srWindow.Left = srWindow.Right - (activeWindowSize.X - 1);
        srWindow.Bottom = max(dwCursorPosition.Y, activeWindowSize.Y - 1);
        srWindow.Top = srWindow.Bottom - (activeWindowSize.Y - 1);
        SetConsoleWindowInfo(cn[startupOutput].handle, TRUE, &srWindow);
    }

    SetConsoleActiveScreenBuffer(cn[startupOutput].handle);
    for (auto &c : cn)
        if (c.owning)
            CloseHandle(c.handle);
    if (ownsConsole)
        FreeConsole();
}

void ConsoleCtl::write(const char *data, size_t bytes) const noexcept
{
    // Запись нуля байтов ненадолго скрывает курсор в старых версиях консоли
    // Windows.
    if (bytes != 0)
        WriteConsoleA(out(), data, bytes, nullptr, nullptr);
}

TPoint ConsoleCtl::getSize() const noexcept
{
    CONSOLE_SCREEN_BUFFER_INFO sbInfo;
    auto &srWindow = sbInfo.srWindow;
    if (GetConsoleScreenBufferInfo(out(), &sbInfo))
        return {
            max(srWindow.Right - srWindow.Left + 1, 0),
            max(srWindow.Bottom - srWindow.Top + 1, 0),
        };
    return {0, 0};
}

TPoint ConsoleCtl::getFontSize() const noexcept
{
    CONSOLE_FONT_INFO fontInfo;
    if (GetCurrentConsoleFont(out(), FALSE, &fontInfo))
        return {
            fontInfo.dwFontSize.X,
            fontInfo.dwFontSize.Y,
        };
    return {0, 0};
}

} // пространство имён tvision

#endif // _TV_UNIX
