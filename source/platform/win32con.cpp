#define Uses_TEvent
#define Uses_TKeys
#define Uses_THardwareInfo
#include <tvision/tv.h>
#include <internal/win32con.h>
#include <internal/conctl.h>
#include <internal/winwidth.h>
#include <internal/codepage.h>
#include <internal/termio.h>
#include <internal/utf8.h>
#include <internal/constmap.h>
#include <locale.h>

namespace tvision
{

#ifdef _WIN32

#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

template <class FuncPtr = FARPROC>
static FuncPtr getModuleProc(const char *moduleName, const char *symbolName)
{
    if (HMODULE mod = GetModuleHandleA(moduleName))
        return (FuncPtr) GetProcAddress(mod, symbolName);
    return nullptr;
}

#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

static bool isWine() noexcept
{
    return getModuleProc("NTDLL", "wine_get_version") != nullptr;
}

Win32ConsoleAdapter &Win32ConsoleAdapter::create() noexcept
{
    auto &con = ConsoleCtl::getInstance();
    DWORD startupMode = initInputMode(con);
    bool isLegacyConsole = initOutputMode(con);
    if (isLegacyConsole)
        disableBitmapFont(con);
    UINT cpInput, cpOutput;
    initEncoding(isLegacyConsole, cpInput, cpOutput);
    WinWidth::reset(isLegacyConsole);
    auto &display = *new Win32Display(con, isLegacyConsole);
    auto &input = *new Win32Input(con);
    return *new Win32ConsoleAdapter(con, startupMode, cpInput, cpOutput, display, input);
}

Win32ConsoleAdapter::~Win32ConsoleAdapter()
{
    delete &display;
    delete &input;
    SetConsoleCP(cpInput);
    SetConsoleOutputCP(cpOutput);
    SetConsoleMode(con.in(), startupMode);
}

DWORD Win32ConsoleAdapter::initInputMode(ConsoleCtl &con) noexcept
{
    DWORD consoleMode = 0;
    GetConsoleMode(con.in(), &consoleMode);
    DWORD startupMode = consoleMode;
    consoleMode |= ENABLE_WINDOW_INPUT; // Сообщать об изменении размера буфера.
    consoleMode |= ENABLE_MOUSE_INPUT; // Сообщать о событиях мыши.
    consoleMode &= ~ENABLE_PROCESSED_INPUT; // Сообщать о Ctrl+C и Shift+стрелка.
    consoleMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT); // Сообщать о Ctrl+S.
    consoleMode |= ENABLE_EXTENDED_FLAGS;   /* Отключаем режим Quick Edit, */
    consoleMode &= ~ENABLE_QUICK_EDIT_MODE; /* который блокирует мышь. */
    SetConsoleMode(con.in(), consoleMode);
    return startupMode;
}

bool Win32ConsoleAdapter::initOutputMode(ConsoleCtl &con) noexcept
{
    DWORD consoleMode = 0;
    GetConsoleMode(con.out(), &consoleMode);
    consoleMode &= ~ENABLE_WRAP_AT_EOL_OUTPUT; // Не прокручиваем экран при достижении конца строки.
    SetConsoleMode(con.out(), consoleMode);

    bool isLegacyConsole;
    if (isWine())
        // Wine не совсем старая консоль, но VT-последовательности не
        // поддерживает, хотя установка ENABLE_VIRTUAL_TERMINAL_PROCESSING не
        // выдаёт ошибок. Поэтому обрабатываем отдельно.
        //
        isLegacyConsole = true;
    else
    {
        consoleMode |= DISABLE_NEWLINE_AUTO_RETURN; // Не выполнять возврат каретки при LF.
        consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // Разрешить управляющие последовательности ANSI.
        SetConsoleMode(con.out(), consoleMode);
        GetConsoleMode(con.out(), &consoleMode);
        // Старая консоль не поддерживает VT-последовательности.
        isLegacyConsole = (consoleMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0;
    }
    return isLegacyConsole;
}

void Win32ConsoleAdapter::initEncoding(bool isLegacyConsole, UINT &cpInput, UINT &cpOutput) noexcept
{
    cpInput = GetConsoleCP();
    cpOutput = GetConsoleOutputCP();
    // Желательно установить UTF-8 для всех кодовых страниц консоли, но
    // старая консоль с растровым шрифтом не умеет правильно показывать
    // UTF-8. С кодовой страницей OEM, обычно поддерживаемой стандартным
    // растровым шрифтом, неподдерживаемые символы могут автоматически
    // заменяться похожими: например ╪ заменяется на ╬ в OEM 850. Для
    // нерастрового шрифта, например Lucida Console, выходная кодовая
    // страница значения не имеет.
    //
    SetConsoleCP(CP_UTF8);
    if (isLegacyConsole)
        SetConsoleOutputCP(GetOEMCP());
    else
        SetConsoleOutputCP(CP_UTF8);
    // Влияет только на функции среды выполнения C. Нужно вызывать после
    // каждого SetConsoleCP().
    setlocale(LC_ALL, ".utf8");
}

bool Win32ConsoleAdapter::isBitmapFont(UINT fontFamily) noexcept
{
    // https://docs.microsoft.com/en-us/windows/console/console-font-infoex:
    // для FontFamily см. описание tmPitchAndFamily структуры TEXTMETRIC.
    // https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-textmetricw:
    // у моноширинного растрового шрифта все младшие биты сброшены.
    //
    return (fontFamily & (TMPF_FIXED_PITCH | TMPF_VECTOR | TMPF_TRUETYPE | TMPF_DEVICE)) == 0;
}

void Win32ConsoleAdapter::disableBitmapFont(ConsoleCtl &con) noexcept
// В старой консоли пытаемся установить нерастровый шрифт, чтобы
// уменьшить вероятность ошибок отображения.
{
    // Функции появились в Windows Vista, поэтому проверяем наличие во время
    // выполнения.
    static auto pGetCurrentConsoleFontEx =
        getModuleProc<decltype(&GetCurrentConsoleFontEx)>("KERNEL32", "GetCurrentConsoleFontEx");
    static auto pSetCurrentConsoleFontEx =
        getModuleProc<decltype(&SetCurrentConsoleFontEx)>("KERNEL32", "SetCurrentConsoleFontEx");

    if (!pGetCurrentConsoleFontEx || !pSetCurrentConsoleFontEx)
        // Функции отсутствуют; сделать ничего нельзя.
        return;

    CONSOLE_FONT_INFOEX fontInfo {};
    fontInfo.cbSize = sizeof(fontInfo);
    if ( !pGetCurrentConsoleFontEx(con.out(), FALSE, &fontInfo) ||
         !isBitmapFont(fontInfo.FontFamily) )
        // Растровый шрифт уже не используется; действий не требуется.
        return;

    // Вычисляем новую высоту по размеру растрового шрифта.
    short fontY = 2*min(fontInfo.dwFontSize.X, fontInfo.dwFontSize.Y);
    for (auto *name : {L"Consolas", L"Lucida Console"})
    {
        fontInfo.nFont = 0;
        fontInfo.FontFamily = FF_DONTCARE;
        fontInfo.FontWeight = FW_NORMAL;
        fontInfo.dwFontSize = {0, fontY}; // Похоже, ширина определяется автоматически.
        wcscpy(fontInfo.FaceName, name);
        // SetCurrentConsoleFontEx успешна даже при отсутствии шрифта. Проверяем,
        // действительно ли он установлен.
        pSetCurrentConsoleFontEx(con.out(), FALSE, &fontInfo);
        pGetCurrentConsoleFontEx(con.out(), FALSE, &fontInfo);
        if (wcscmp(fontInfo.FaceName, name) == 0)
            // Шрифт успешно изменён.
            return;
    }
}

bool Win32ConsoleAdapter::isAlive() noexcept
{
    DWORD events = 0;
    return GetNumberOfConsoleInputEvents(con.in(), &events);
}

static bool openClipboard() noexcept
{
    for (int i = 0; i < 5; ++i)
    {
        if (OpenClipboard(nullptr))
            return true;
        Sleep(5);
    }
    return false;
}

bool Win32ConsoleAdapter::setClipboardText(TStringView text) noexcept
{
    bool result = false;
    if (openClipboard())
    {
        HGLOBAL hData = NULL;
        wchar_t *pData;
        int dataLen;
        if ( EmptyClipboard() &&
             !(result = text.empty()) &&
             (dataLen = MultiByteToWideChar(CP_UTF8, 0, text.data(), text.size(), nullptr, 0)) &&
             (hData = GlobalAlloc(GMEM_MOVEABLE, (dataLen + 1)*sizeof(wchar_t))) &&
             (pData = (wchar_t *) GlobalLock(hData))
           )
        {
            MultiByteToWideChar(CP_UTF8, 0, text.data(), text.size(), pData, dataLen);
            pData[dataLen] = L'\0';
            GlobalUnlock(hData);
            result = SetClipboardData(CF_UNICODETEXT, hData);
        }
        CloseClipboard();
        if (hData && !result)
            GlobalFree(hData);
    }
    return result;
}

bool Win32ConsoleAdapter::requestClipboardText(void (&accept)(TStringView)) noexcept
{
    bool result = false;
    if (openClipboard())
    {
        HGLOBAL hData;
        wchar_t *pData;
        if ( (hData = GetClipboardData(CF_UNICODETEXT)) &&
             (result = (pData = (wchar_t *) GlobalLock(hData))) )
        {
            size_t dataLen = wcslen(pData);
            int textLen = WideCharToMultiByte(CP_UTF8, 0, pData, dataLen, nullptr, 0, nullptr, nullptr);
            char *text = new char[textLen];
            WideCharToMultiByte(CP_UTF8, 0, pData, dataLen, text, textLen, nullptr, nullptr);
            GlobalUnlock(hData);
            accept({text, size_t(textLen)});
            delete[] text;
        }
        CloseClipboard();
    }
    return result;
}

/////////////////////////////////////////////////////////////////////////
// Win32Input

Win32Input::Win32Input(ConsoleCtl &aCon) noexcept :
    InputAdapter(aCon.in()),
    con(aCon)
{
}

bool Win32Input::getEvent(TEvent &ev) noexcept
{
    // ReadConsoleInput может заблокировать процесс, поэтому сначала
    // проверяем число доступных событий.
    DWORD events;
    while (GetNumberOfConsoleInputEvents(con.in(), &events) && events)
    {
        // getEvent(ir, ev) часто возвращает false из-за отбрасываемых событий.
        // Но при наличии ожидающих событий эта функция не должна возвращать
        // false: иначе нарушится работа очереди THardwareInfo.
        while (events--)
        {
            INPUT_RECORD ir;
            DWORD ok;
            if (!ReadConsoleInputW(con.in(), &ir, 1, &ok) || !ok)
                return false;
            if (getEvent(ir, ev))
                return true;
        }
    }
    return false;
}

bool Win32Input::getEvent(const INPUT_RECORD &ir, TEvent &ev) noexcept
{
    switch (ir.EventType)
    {
    case KEY_EVENT:
        if ( ir.Event.KeyEvent.bKeyDown || // Нажатие клавиши.
             (ir.Event.KeyEvent.wVirtualKeyCode == VK_MENU && // Суррогатный символ из вставки.
                ir.Event.KeyEvent.uChar.UnicodeChar) )
            return getWin32Key(ir.Event.KeyEvent, ev, state);
        break;
    case MOUSE_EVENT:
        getWin32Mouse(ir.Event.MouseEvent, ev, state);
        return true;
    case WINDOW_BUFFER_SIZE_EVENT:
        ev.what = evCommand;
        ev.message.command = cmScreenChanged;
        ev.message.infoPtr = 0;
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////
// Win32Display

Win32Display::Win32Display(ConsoleCtl &aCon, bool isLegacyConsole) noexcept :
    con(aCon),
    legacyConsole(isLegacyConsole)
{
    if (!isLegacyConsole)
        ansiScreenWriter = new AnsiScreenWriter(con, TermCap::getDisplayCapabilities(con, *this));
}

Win32Display::~Win32Display()
{
    delete ansiScreenWriter;
}

TPoint Win32Display::reloadScreenInfo() noexcept
{
    TPoint lastSize = size;
    size = con.getSize();

    if (lastSize != size)
    {
        CONSOLE_SCREEN_BUFFER_INFO sbInfo {};
        GetConsoleScreenBufferInfo(con.out(), &sbInfo);
        // Временно перемещаем курсор в (0, 0), предотвращая сбой консоли из-за
        // https://github.com/microsoft/terminal/issues/7511.
        auto curPos = sbInfo.dwCursorPosition;
        SetConsoleCursorPosition(con.out(), {0, 0});
        // Размер буфера должен совпадать с областью просмотра, чтобы не было
        // полос прокрутки. Меняем его только при изменении области, иначе, по
        // крайней мере в Wine, будут постоянно возникать
        // WINDOW_BUFFER_SIZE_EVENT.
        //
        SetConsoleScreenBufferSize(con.out(), {(short) size.x, (short) size.y});
        // Восстанавливаем позицию курсора; выход за границы не важен.
        SetConsoleCursorPosition(con.out(), curPos);
    }

    CONSOLE_FONT_INFO fontInfo;
    if ( GetCurrentConsoleFont(con.out(), FALSE, &fontInfo)
         && memcmp(&fontInfo, &lastFontInfo, sizeof(fontInfo)) != 0 )
    {
        // Ширина символа зависит от шрифта и его размера.
        WinWidth::reset(ansiScreenWriter == nullptr);
        lastFontInfo = fontInfo;
    }

    if (ansiScreenWriter)
        ansiScreenWriter->reset();
    else
    {
        caretPos = {-1, -1};
        lastAttr = '\x00';
    }

    return size;
}

int Win32Display::getColorCount() noexcept
{
    // https://devblogs.microsoft.com/commandline/24-bit-color-in-the-windows-console/.
    // В современной консоли предполагаем поддержку расширенных цветов.
    // Запрос выполняется и во время создания ansiScreenWriter.
    if (!legacyConsole)
        return 256*256*256;
    return 16;
}

TPoint Win32Display::getFontSize() noexcept
{
    return con.getFontSize();
}

void Win32Display::setCaretSize(int size) noexcept
{
    CONSOLE_CURSOR_INFO crInfo;
    if (size) {
        crInfo.bVisible = TRUE;
        crInfo.dwSize = size;
    } else {
        crInfo.bVisible = FALSE;
        crInfo.dwSize = 1;
    }
    SetConsoleCursorInfo(con.out(), &crInfo);
}

void Win32Display::clearScreen() noexcept
{
    if (ansiScreenWriter)
        ansiScreenWriter->clearScreen();
    else
    {
        COORD coord = {0, 0};
        DWORD length = size.x * size.y;
        BYTE attr = 0x07;
        DWORD read;
        FillConsoleOutputAttribute(con.out(), attr, length, coord, &read);
        FillConsoleOutputCharacterA(con.out(), ' ', length, coord, &read);
        lastAttr = attr;
    }
}

void Win32Display::writeCell( TPoint pos, TStringView text, TColorAttr attr,
                              bool doubleWidth ) noexcept
{
    if (ansiScreenWriter)
        ansiScreenWriter->writeCell(pos, text, attr, doubleWidth);
    else
    {
        if (pos != caretPos)
        {
            flush();
            SetConsoleCursorPosition(con.out(), {(short) pos.x, (short) pos.y});
        }

        uchar biosAttr = attr.toBIOS();
        if (biosAttr != lastAttr)
        {
            flush();
            SetConsoleTextAttribute(con.out(), biosAttr);
        }

        buf.insert(buf.end(), text.begin(), text.end());

        caretPos = {pos.x + 1 + doubleWidth, pos.y};
        lastAttr = biosAttr;
    }
}

void Win32Display::setCaretPosition(TPoint pos) noexcept
{
    if (ansiScreenWriter)
        ansiScreenWriter->setCaretPosition(pos);
    else
    {
        flush();
        SetConsoleCursorPosition(con.out(), {(short) pos.x, (short) pos.y});
        caretPos = pos;
    }
}

void Win32Display::flush() noexcept
{
    if (ansiScreenWriter)
        ansiScreenWriter->flush();
    else if (!buf.empty())
    {
        // В старой консоли с растровым шрифтом нужна кодовая страница OEM; см.
        // initEncoding(). WriteConsoleW неверно обрабатывает UTF-16-символы,
        // соответствующие управляющим кодам OEM: например ○ превращается в 0x09,
        // то есть табуляцию. Поэтому используем WriteConsoleOutputW. Для
        // нерастрового шрифта OEM не нужна, а WriteConsoleW и
        // WriteConsoleOutputW эквивалентны; отдельная логика не требуется.
        // Каждый wchar_t, то есть кодовая единица UTF-16, занимает одну ячейку.
        //
        //
        //
        int wCharCount = MultiByteToWideChar(CP_UTF8, 0, &buf[0], buf.size(), nullptr, 0);
        std::vector<wchar_t> wChars(wCharCount);
        MultiByteToWideChar(CP_UTF8, 0, &buf[0], buf.size(), &wChars[0], wCharCount);

        std::vector<CHAR_INFO> cells(wCharCount);
        for (int i = 0; i < wCharCount; ++i)
        {
            cells[i].Char.UnicodeChar = wChars[i];
            cells[i].Attributes = lastAttr;
        }

        SMALL_RECT to = {
            short(caretPos.x - wCharCount),
            short(caretPos.y),
            short(caretPos.x - 1),
            short(caretPos.y),
        };
        WriteConsoleOutputW(con.out(), &cells[0], {(short) wCharCount, 1}, {0, 0}, &to);

        buf.resize(0);
    }
}

#endif // _WIN32

// Методы Global
//

static bool getWin32KeyText(const KEY_EVENT_RECORD &KeyEvent, TEvent &ev, InputState &state) noexcept
// Возвращает true, кроме события с суррогатом UTF-16 в Windows: тогда
// нужно следующее событие.
{
    uint32_t ch = KeyEvent.uChar.UnicodeChar;
    ev.keyDown.textLength = 0;

    // Не считаем непечатные символы текстом.
    if (' ' <= ch && ch != 0x7F)
    {
#ifdef _WIN32
        if (0xD800 <= ch && ch <= 0xDBFF)
        {
            state.surrogate = ch;
            return false;
        }

        wchar_t utf16[2] = {(wchar_t) ch, 0};
        if (state.surrogate)
        {
            if (0xDC00 <= ch && ch <= 0xDFFF)
            {
                utf16[1] = (wchar_t) ch;
                utf16[0] = state.surrogate;
            }
            state.surrogate = 0;
        }

        ev.keyDown.textLength = WideCharToMultiByte(
            CP_UTF8, 0,
            utf16, utf16[1] ? 2 : 1,
            ev.keyDown.text, sizeof(ev.keyDown.text),
            nullptr, nullptr
        );
#else
        (void) state;

        if (ch < 0xD800 || (0xDFFF < ch && ch < 0x10FFFF))
            ev.keyDown.textLength = (uchar) utf32To8(ch, ev.keyDown.text);
#endif // _WIN32
    }
    return true;
}

bool getWin32Key(const KEY_EVENT_RECORD &KeyEvent, TEvent &ev, InputState &state) noexcept
{
    if (!getWin32KeyText(KeyEvent, ev, state))
        return false;

    ev.what = evKeyDown;
    ev.keyDown.charScan.scanCode = KeyEvent.wVirtualScanCode;
    ev.keyDown.charScan.charCode = KeyEvent.uChar.AsciiChar;
    ev.keyDown.controlKeyState = KeyEvent.dwControlKeyState & (
        kbShift | kbCtrlShift | kbAltShift |
        kbScrollState | kbNumState | kbCapsState | kbEnhanced
    );

    if (ev.keyDown.textLength != 0)
    {
        ev.keyDown.charScan.charCode = CpTranslator::fromUtf8(ev.keyDown.getText());
        if (KeyEvent.wVirtualKeyCode == VK_MENU)
            // Включается при вставке некоторых символов и сбивает приложения.
            // Сбрасываем.
            ev.keyDown.charScan.scanCode = 0;
        if (ev.keyDown.charScan.charCode == '\0' || ev.keyDown.keyCode <= kbCtrlZ)
            // Если символ не представим в текущей кодовой странице либо случайно
            // запускает Ctrl+клавиша, обнуляем весь keyCode, избегая побочных
            // эффектов.
            ev.keyDown.keyCode = kbNoKey;
    }

    if ( ev.keyDown.keyCode == 0x2A00 || ev.keyDown.keyCode == 0x1D00 ||
         ev.keyDown.keyCode == 0x3600 || ev.keyDown.keyCode == 0x3800 ||
         ev.keyDown.keyCode == 0x3A00 || ev.keyDown.keyCode == 0x5B00 ||
         ev.keyDown.keyCode == 0x5C00 )
        // Отбрасываем самостоятельные Shift, Ctrl, Alt, Caps Lock и Windows.
        ev.keyDown.keyCode = kbNoKey;
    else if ( (ev.keyDown.controlKeyState & kbLeftCtrl) &&
              (ev.keyDown.controlKeyState & kbRightAlt) &&
              ev.keyDown.textLength == 0 )
        // Нельзя уверенно определить, является ли правый Alt клавишей AltGr: это
        // зависит от раскладки. Но AltGr всегда добавляет флаг левого Ctrl. Если
        // оба флага установлены, а текста нет, отбрасываем событие, чтобы не
        // трактовать AltGr как Ctrl+Alt.
        //
        ev.keyDown.keyCode = kbNoKey;
    else if ( (ev.keyDown.controlKeyState & kbCtrlShift) &&
              (ev.keyDown.controlKeyState & kbAltShift) &&
              ev.keyDown.textLength != 0 )
        // Если Ctrl+Alt создаёт текст, это AltGr. Отбрасываем модификаторы Ctrl
        // и Alt.
        ev.keyDown.controlKeyState &= ~(kbCtrlShift | kbAltShift);
    else if (KeyEvent.wVirtualScanCode < 89)
    {
        // Преобразуем виртуальные скан-коды NT в коды PC BIOS.
        uchar index = KeyEvent.wVirtualScanCode;
        ushort keyCode = 0;
        if ((ev.keyDown.controlKeyState & kbAltShift) && THardwareInfo::AltCvt[index])
            keyCode = THardwareInfo::AltCvt[index];
        else if ((ev.keyDown.controlKeyState & kbCtrlShift) && THardwareInfo::CtrlCvt[index])
            keyCode = THardwareInfo::CtrlCvt[index];
        else if ((ev.keyDown.controlKeyState & kbShift) && THardwareInfo::ShiftCvt[index])
            keyCode = THardwareInfo::ShiftCvt[index];
        else if ( !(ev.keyDown.controlKeyState & (kbShift | kbCtrlShift | kbAltShift)) &&
                  THardwareInfo::NormalCvt[index] )
            keyCode = THardwareInfo::NormalCvt[index];

        if (keyCode != 0)
        {
            ev.keyDown.keyCode = keyCode;
            if (ev.keyDown.charScan.charCode < ' ')
                ev.keyDown.textLength = 0;
            else if (ev.keyDown.charScan.charCode < 0x7F && !ev.keyDown.textLength)
            {
                ev.keyDown.text[0] = ev.keyDown.charScan.charCode;
                ev.keyDown.textLength = 1;
            }
        }
    }

    return ev.keyDown.keyCode != kbNoKey || ev.keyDown.textLength;
}

void getWin32Mouse(const MOUSE_EVENT_RECORD &MouseEvent, TEvent &ev, InputState &state) noexcept
{
    ev.what = evMouse;
    ev.mouse.where.x = MouseEvent.dwMousePosition.X;
    ev.mouse.where.y = MouseEvent.dwMousePosition.Y;
    ev.mouse.buttons = state.buttons = MouseEvent.dwButtonState;
    ev.mouse.eventFlags = MouseEvent.dwEventFlags;
    ev.mouse.controlKeyState = MouseEvent.dwControlKeyState & (
        kbShift | kbCtrlShift | kbAltShift |
        kbScrollState | kbNumState | kbCapsState | kbEnhanced
    );

    // Направление вращения задаётся знаком старшего слова dwButtonState.
    Boolean positive = !(MouseEvent.dwButtonState & 0x80000000);
    if( MouseEvent.dwEventFlags & MOUSE_WHEELED )
        ev.mouse.wheel = positive ? mwUp : mwDown;
    else if( MouseEvent.dwEventFlags & MOUSE_HWHEELED )
        ev.mouse.wheel = positive ? mwRight : mwLeft;
    else
        ev.mouse.wheel = 0;
}

static const const_unordered_map<uchar, ushort> virtualKeyCodeToTVisionKeyCode =
{
    { VK_BACK,      kbBack       }, { VK_TAB,       kbTab        },
    { VK_RETURN,    kbEnter      }, { VK_ESCAPE,    kbEsc        },
    { '0',          0x0B00 | '0' }, { '1',          0x0200 | '1' },
    { '2',          0x0300 | '2' }, { '3',          0x0400 | '3' },
    { '4',          0x0500 | '4' }, { '5',          0x0600 | '5' },
    { '6',          0x0700 | '6' }, { '7',          0x0800 | '7' },
    { '8',          0x0900 | '8' }, { '9',          0x0A00 | '9' },
    { VK_SPACE,     0x3900 | ' ' }, { 'A',          0x1E00 | 'A' },
    { 'B',          0x3000 | 'B' }, { 'C',          0x2E00 | 'C' },
    { 'D',          0x2000 | 'D' }, { 'E',          0x1200 | 'E' },
    { 'F',          0x2100 | 'F' }, { 'G',          0x2200 | 'G' },
    { 'H',          0x2300 | 'H' }, { 'I',          0x1700 | 'I' },
    { 'J',          0x2400 | 'J' }, { 'K',          0x2500 | 'K' },
    { 'L',          0x2600 | 'L' }, { 'M',          0x3200 | 'M' },
    { 'N',          0x3100 | 'N' }, { 'O',          0x1800 | 'O' },
    { 'P',          0x1900 | 'P' }, { 'Q',          0x1000 | 'Q' },
    { 'R',          0x1300 | 'R' }, { 'S',          0x1F00 | 'S' },
    { 'T',          0x1400 | 'T' }, { 'U',          0x1600 | 'U' },
    { 'V',          0x2F00 | 'V' }, { 'W',          0x1100 | 'W' },
    { 'X',          0x2D00 | 'X' }, { 'Y',          0x1500 | 'Y' },
    { 'Z',          0x2C00 | 'Z' }, { VK_PRIOR,     kbPgUp       },
    { VK_NEXT,      kbPgDn       }, { VK_END,       kbEnd        },
    { VK_HOME,      kbHome       }, { VK_LEFT,      kbLeft       },
    { VK_UP,        kbUp         }, { VK_RIGHT,     kbRight      },
    { VK_DOWN,      kbDown       }, { VK_INSERT,    kbIns        },
    { VK_DELETE,    kbDel        }, { VK_NUMPAD0,   0x5200 | '0' },
    { VK_NUMPAD1,   0x4F00 | '1' }, { VK_NUMPAD2,   0x5000 | '2' },
    { VK_NUMPAD3,   0x5100 | '3' }, { VK_NUMPAD4,   0x4B00 | '4' },
    { VK_NUMPAD5,   0x4C00 | '5' }, { VK_NUMPAD6,   0x4D00 | '6' },
    { VK_NUMPAD7,   0x4700 | '7' }, { VK_NUMPAD8,   0x4800 | '8' },
    { VK_NUMPAD9,   0x4900 | '9' }, { VK_MULTIPLY,  0x3700 | '*' },
    { VK_ADD,       0x4E00 | '+' }, { VK_SEPARATOR, 0x7E00 | ',' },
    { VK_SUBTRACT,  0x4A00 | '-' }, { VK_DECIMAL,   0x5300 | '.' },
    { VK_DIVIDE,    0x3500 | '/' }, { VK_F1,        kbF1         },
    { VK_F2,        kbF2         }, { VK_F3,        kbF3         },
    { VK_F4,        kbF4         }, { VK_F5,        kbF5         },
    { VK_F6,        kbF6         }, { VK_F7,        kbF7         },
    { VK_F8,        kbF8         }, { VK_F9,        kbF9         },
    { VK_F10,       kbF10        }, { VK_F11,       kbF11        },
    { VK_F12,       kbF12        },
};

void regenerateMissingScanCodeFromVirtualKeyCode(KEY_EVENT_RECORD &KeyEvent) noexcept
{
    if (uint16_t keyCode = virtualKeyCodeToTVisionKeyCode[KeyEvent.wVirtualKeyCode])
    {
        KeyEvent.wVirtualScanCode = keyCode >> 8;
        // Перезаписываем UnicodeChar, кроме случая, когда он должен быть
        // ненулевым и уже содержит значение.
        if (KeyEvent.uChar.UnicodeChar == L'\0' || (keyCode & 0xFF) == 0)
            KeyEvent.uChar.UnicodeChar = keyCode & 0xFF;

        // При наличии Ctrl или Alt событие не создаёт текст, поэтому очищаем
        // UnicodeChar. В отличие от Windows, здесь не ожидаем представления
        // AltGr как Ctrl+Alt.
        if ( (KeyEvent.dwControlKeyState & (kbCtrlShift | kbAltShift)) &&
             L' ' <= KeyEvent.uChar.UnicodeChar &&
             L'\x7F' != KeyEvent.uChar.UnicodeChar )
            KeyEvent.uChar.UnicodeChar = L'\0';
    }
}

} // пространство имён tvision

