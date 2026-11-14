/*------------------------------------------------------------*/
/* файл — hardwrvr.cpp */
/*                                                            */
/* функции:  */
/* Переменные-члены класса THardwareInfo. */
/*                                                            */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TKeys
#define Uses_TEvent
#define Uses_TScreen
#define Uses_THardwareInfo
#define Uses_TSystemError
#include <tvision/tv.h>
#include <dos.h>
#include <iostream.h>

#if defined( __FLAT__ )

BOOL THardwareInfo::insertState = True;
THardwareInfo::PlatformType THardwareInfo::platform = THardwareInfo::plDPMI32;
HANDLE THardwareInfo::consoleHandle[3];
DWORD THardwareInfo::consoleMode;
DWORD THardwareInfo::pendingEvent;
INPUT_RECORD THardwareInfo::irBuffer;
CONSOLE_CURSOR_INFO THardwareInfo::crInfo;
CONSOLE_SCREEN_BUFFER_INFO THardwareInfo::sbInfo;

const ushort THardwareInfo::NormalCvt[89] = {
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0, 0x8500,
    0x8600
};

const ushort THardwareInfo::ShiftCvt[89] = {
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0, 0x0F00,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0,      0, 0x5400, 0x5500, 0x5600, 0x5700, 0x5800,
    0x5900, 0x5A00, 0x5B00, 0x5C00, 0x5D00,      0,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0,
         0,      0, 0x0500, 0x0700,      0,      0,      0, 0x8700,
    0x8800
};

const ushort THardwareInfo::CtrlCvt[89] = {
         0,      0, 0x0231, 0x0332, 0x0433, 0x0534, 0x0635, 0x0736,
    0x0837, 0x0938, 0x0A39, 0x0B30,      0,      0,      0, 0x9400,
    0x0011, 0x0017, 0x0005, 0x0012, 0x0014, 0x0019, 0x0015, 0x0009,
    0x000F, 0x0010,      0,      0,      0,      0, 0x0001, 0x0013,
    0x0004, 0x0006, 0x0007, 0x0008, 0x000A, 0x000B, 0x000C,      0,
         0,      0,      0,      0, 0x001A, 0x0018, 0x0003, 0x0016,
    0x0002, 0x000E, 0x000D,      0,      0, 0x352F,      0, 0x372A,
         0,      0,      0, 0x5E00, 0x5F00, 0x6000, 0x6100, 0x6200,
    0x6300, 0x6400, 0x6500, 0x6600, 0x6700,      0,      0, 0x7700,
    0x8D00, 0x8400, 0x4A2D, 0x7300,      0, 0x7400, 0x4E2B, 0x7500,
    0x9100, 0x7600, 0x0400, 0x0600,      0,      0,      0, 0x8900,
    0x8A00
};

const ushort THardwareInfo::AltCvt[89] = {
         0, 0x0100, 0x7800, 0x7900, 0x7A00, 0x7B00, 0x7C00, 0x7D00,
    0x7E00, 0x7F00, 0x8000, 0x8100, 0x8200, 0x8300, 0x0E00, 0xA500,
    0x1000, 0x1100, 0x1200, 0x1300, 0x1400, 0x1500, 0x1600, 0x1700,
    0x1800, 0x1900,      0,      0, 0xA600,      0, 0x1E00, 0x1F00,
    0x2000, 0x2100, 0x2200, 0x2300, 0x2400, 0x2500, 0x2600,      0,
         0,      0,      0,      0, 0x2C00, 0x2D00, 0x2E00, 0x2F00,
    0x3000, 0x3100, 0x3200,      0,      0,      0,      0,      0,
         0, 0x0200,      0, 0x6800, 0x6900, 0x6A00, 0x6B00, 0x6C00,
    0x6D00, 0x6E00, 0x6F00, 0x7000, 0x7100,      0,      0, 0x9700,
    0x9800, 0x9900, 0x8200, 0x9B00,      0, 0x9D00,      0, 0x9F00,
    0xA000, 0xA100, 0xA200, 0xA300,      0,      0,      0, 0x8B00,
    0x8C00
};

#else

Boolean THardwareInfo::dpmiFlag;
ushort THardwareInfo::colorSel;
ushort THardwareInfo::monoSel;
ushort THardwareInfo::biosSel;

#endif

#if defined( __FLAT__ )

#ifdef __BORLANDC__

#define INT10   { __emit__( 0xCD ); __emit__( 0x10 ); }

static void *getModuleProc(const char *modName, const char *procName)
{
    HMODULE mod = GetModuleHandle(modName);
    return mod != 0 ? GetProcAddress(mod, procName) : 0;
};

// Методы недоступны в DPMI32 и не допускают статической компоновки.

static BOOL (WINAPI * pSetConsoleActiveScreenBuffer)(HANDLE);
static BOOL (WINAPI * pOpenClipboard)(HWND);
static BOOL (WINAPI * pCloseClipboard)();
static HANDLE (WINAPI * pSetClipboardData)(UINT, HANDLE);
static HANDLE (WINAPI * pGetClipboardData)(UINT);
static BOOL (WINAPI * pEmptyClipboard)();

// Конструктор 16-битной версии находится в HARDWARE.ASM.

THardwareInfo::THardwareInfo()
{
    if( getModuleProc("KERNEL32", "Borland32") != 0 )
        platform = plDPMI32;
    else
        platform = plWinNT;

    consoleHandle[cnInput] = GetStdHandle( STD_INPUT_HANDLE );
    consoleHandle[cnOutput] = GetStdHandle( STD_OUTPUT_HANDLE );
    if (!GetConsoleMode( consoleHandle[cnInput], &consoleMode ))
        {
        cerr << "Error: standard input is being redirected or is not a "
                "Win32 console." << endl;
        ExitProcess(1);
        }
    GetConsoleCursorInfo( consoleHandle[cnOutput], &crInfo );
    GetConsoleScreenBufferInfo( consoleHandle[cnOutput], &sbInfo );

    if( platform == plWinNT )
        {
        consoleHandle[cnStartup] = consoleHandle[cnOutput];
        consoleHandle[cnOutput] = CreateConsoleScreenBuffer(
            GENERIC_READ | GENERIC_WRITE,
            0,
            0,
            CONSOLE_TEXTMODE_BUFFER,
            0);
        // Приводим размер экранного буфера к размеру окна. API консоли
        // гарантирует это, но некоторые реализации, например Wine, нарушают
        // условие.
        sbInfo.dwSize.X = sbInfo.srWindow.Right - sbInfo.srWindow.Left + 1;
        sbInfo.dwSize.Y = sbInfo.srWindow.Bottom - sbInfo.srWindow.Top + 1;
        SetConsoleScreenBufferSize(consoleHandle[cnOutput], sbInfo.dwSize);
        }

    consoleMode |= ENABLE_WINDOW_INPUT; // Сообщать об изменении размера буфера.
    consoleMode &= ~ENABLE_PROCESSED_INPUT; // Сообщать о Ctrl+C и Shift+стрелка.
    consoleMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT); // Сообщать о Ctrl+S.
    SetConsoleMode( consoleHandle[cnInput], consoleMode );
    // Эти флаги появились в поздних версиях Windows. На случай ошибки
    // используем отдельный SetConsoleMode.
    consoleMode |= ENABLE_EXTENDED_FLAGS;   /* Отключаем режим Quick Edit, */
    consoleMode &= ~ENABLE_QUICK_EDIT_MODE; /* который блокирует мышь. */
    SetConsoleMode( consoleHandle[cnInput], consoleMode );

    pSetConsoleActiveScreenBuffer =
        (BOOL (WINAPI *)(HANDLE)) getModuleProc("KERNEL32", "SetConsoleActiveScreenBuffer");
    pOpenClipboard =
        (BOOL (WINAPI *)(HWND)) getModuleProc("USER32", "OpenClipboard");
    pCloseClipboard =
        (BOOL (WINAPI *)()) getModuleProc("USER32", "CloseClipboard");
    pSetClipboardData =
        (HANDLE (WINAPI *)(UINT, HANDLE)) getModuleProc("USER32", "SetClipboardData");
    pGetClipboardData =
        (HANDLE (WINAPI *)(UINT)) getModuleProc("USER32", "GetClipboardData");
    pEmptyClipboard =
        (BOOL (WINAPI *)()) getModuleProc("USER32", "EmptyClipboard");
}

THardwareInfo::~THardwareInfo()
{
    restoreConsole();
    CloseHandle( consoleHandle[cnOutput] );
}

void THardwareInfo::setUpConsole()
{
    if( pSetConsoleActiveScreenBuffer )
        {
        pSetConsoleActiveScreenBuffer( consoleHandle[cnOutput] );
        GetConsoleScreenBufferInfo( consoleHandle[cnOutput], &sbInfo );
        }
}

void THardwareInfo::restoreConsole()
{
    if( pSetConsoleActiveScreenBuffer )
        pSetConsoleActiveScreenBuffer( consoleHandle[cnStartup] );
}

ushort THardwareInfo::getScreenMode()
{
    ushort mode;

    if( platform != plDPMI32 )      // Чёрно-белый и монохромный режимы не поддерживаются при работе в
        mode = TDisplay::smCO80;    // NT. Для этого пришлось бы обращаться к реестру.
    else
        {
        _AH = 0x0F;
        INT10;                      // Выдаём CD, 10. Поддерживается сервером DPMI.
        mode = _AL;
        }

    if( getScreenRows() > 25 )
        mode |= TDisplay::smFont8x8;
    return mode;
}

void THardwareInfo::setScreenMode( ushort mode )
{
    COORD newSize = { 80, 25 };
    SMALL_RECT rect = { 0, 0, 79, 24 };

    if( mode & TDisplay::smFont8x8 )
        {
        newSize.Y = 50;
        rect.Bottom = 49;
        }
    if( platform != plDPMI32 )
        {
        COORD maxSize = GetLargestConsoleWindowSize( consoleHandle[cnOutput] );
        if( newSize.Y > maxSize.Y )
            {
            newSize.Y = maxSize.Y;
            rect.Bottom = newSize.Y-1;
            }
        }

    if( mode & TDisplay::smFont8x8 )
        {
        SetConsoleScreenBufferSize( consoleHandle[cnOutput], newSize );
        SetConsoleWindowInfo( consoleHandle[cnOutput], True, &rect );
        }
    else
        {
        SetConsoleWindowInfo( consoleHandle[cnOutput], True, &rect );
        SetConsoleScreenBufferSize( consoleHandle[cnOutput], newSize );
        }

    GetConsoleScreenBufferInfo( consoleHandle[cnOutput], &sbInfo );
}

void THardwareInfo::setCaretPosition( ushort x, ushort y )
{
    COORD coord = { x, y };
    SetConsoleCursorPosition( consoleHandle[cnOutput], coord );
}

void THardwareInfo::setCaretSize( ushort size )
{
    if( size == 0 )
    {
        crInfo.bVisible = False;
        crInfo.dwSize = 1;
    }
    else
    {
        crInfo.bVisible = True;
        crInfo.dwSize = size;
    }

    SetConsoleCursorInfo( consoleHandle[cnOutput], &crInfo );
}

void THardwareInfo::screenWrite( ushort x, ushort y, CHAR_INFO *buf, DWORD len )
{
    COORD size = {len,1};
    COORD from = {0,0};
    SMALL_RECT to = {x,y,x+len-1,y};

    WriteConsoleOutput( consoleHandle[cnOutput], buf, size, from, &to);
}

// Методы Event

BOOL THardwareInfo::getMouseEvent( MouseEventType& event )
{
    if( !pendingEvent )
        {
        GetNumberOfConsoleInputEvents( consoleHandle[cnInput], &pendingEvent );
        if( pendingEvent )
            ReadConsoleInput( consoleHandle[cnInput], &irBuffer, 1, &pendingEvent );
        }

    if( pendingEvent && irBuffer.EventType == MOUSE_EVENT)
        {
        event.where.x = irBuffer.Event.MouseEvent.dwMousePosition.X;
        event.where.y = irBuffer.Event.MouseEvent.dwMousePosition.Y;
        event.buttons = irBuffer.Event.MouseEvent.dwButtonState;
        event.eventFlags = irBuffer.Event.MouseEvent.dwEventFlags;
        event.controlKeyState = irBuffer.Event.MouseEvent.dwControlKeyState;

        // Направление вращения задаётся знаком старшего слова dwButtonState.
        int positive = !(irBuffer.Event.MouseEvent.dwButtonState & 0x80000000);
        if( irBuffer.Event.MouseEvent.dwEventFlags & MOUSE_WHEELED )
            event.wheel = positive ? mwUp : mwDown;
        else if( irBuffer.Event.MouseEvent.dwEventFlags & MOUSE_HWHEELED )
            event.wheel = positive ? mwRight : mwLeft;
        else
            event.wheel = 0;

        pendingEvent = 0;
        return True;
        }
    return False;
}

BOOL THardwareInfo::getKeyEvent( TEvent& event )
{
    do  {
        if( !pendingEvent )
            {
            GetNumberOfConsoleInputEvents( consoleHandle[cnInput], &pendingEvent );
            if( pendingEvent )
                ReadConsoleInput( consoleHandle[cnInput], &irBuffer, 1, &pendingEvent );
            else
                return False;
            }

        // Ожидающие события мыши будут прочитаны на следующем цикле опроса.
        if( pendingEvent && irBuffer.EventType != MOUSE_EVENT )
            {
            pendingEvent = 0;

            if( irBuffer.EventType == KEY_EVENT && irBuffer.Event.KeyEvent.bKeyDown )
                {
                event.what = evKeyDown;
                event.keyDown.charScan.scanCode = irBuffer.Event.KeyEvent.wVirtualScanCode;
                event.keyDown.charScan.charCode = irBuffer.Event.KeyEvent.uChar.AsciiChar;
                event.keyDown.controlKeyState = irBuffer.Event.KeyEvent.dwControlKeyState;

                if( event.keyDown.keyCode == 0x2A00 || event.keyDown.keyCode == 0x1D00 ||
                    event.keyDown.keyCode == 0x3600 || event.keyDown.keyCode == 0x3800 ||
                    event.keyDown.keyCode == 0x3A00 || event.keyDown.keyCode == 0x5B00 ||
                    event.keyDown.keyCode == 0x5C00 )
                    // Отбрасываем самостоятельные Shift, Ctrl, Alt, Caps Lock и Windows.
                    event.keyDown.keyCode = kbNoKey;
                else if( (event.keyDown.controlKeyState & kbLeftCtrl) &&
                         (event.keyDown.controlKeyState & kbRightAlt) &&
                         event.keyDown.charScan.charCode == '\0' )
                    // Нельзя уверенно определить, является ли правый Alt клавишей AltGr: это
                    // зависит от раскладки. Но AltGr всегда добавляет флаг левого Ctrl. Если
                    // оба флага установлены, а текста нет, отбрасываем событие, чтобы не
                    // трактовать AltGr как Ctrl+Alt.
                    //
                    event.keyDown.keyCode = kbNoKey;
                else if( (event.keyDown.controlKeyState & kbCtrlShift) &&
                         (event.keyDown.controlKeyState & kbAltShift) &&
                         event.keyDown.charScan.charCode != '\0' )
                    // Если Ctrl+Alt создаёт текст, это AltGr. Отбрасываем модификаторы Ctrl
                    // и Alt.
                    event.keyDown.controlKeyState &= ~(kbCtrlShift | kbAltShift);
                else if( irBuffer.Event.KeyEvent.wVirtualScanCode < 89 )
                    {
                    // Преобразуем виртуальные скан-коды NT в коды PC BIOS.
                    uchar index = irBuffer.Event.KeyEvent.wVirtualScanCode;
                    if ((event.keyDown.controlKeyState & kbAltShift) && AltCvt[index] != 0)
                        event.keyDown.keyCode = AltCvt[index];
                    else if ((event.keyDown.controlKeyState & kbCtrlShift) && CtrlCvt[index] != 0)
                        event.keyDown.keyCode = CtrlCvt[index];
                    else if ((event.keyDown.controlKeyState & kbShift) && ShiftCvt[index] != 0)
                        event.keyDown.keyCode = ShiftCvt[index];
                    else if ( !(event.keyDown.controlKeyState & (kbShift | kbCtrlShift | kbAltShift)) &&
                              NormalCvt[index] != 0 )
                        event.keyDown.keyCode = NormalCvt[index];
                    }

                /* Устанавливаем или сбрасываем флаг вставки.
 */
                if( event.keyDown.keyCode == kbIns )
                    insertState = !insertState;

                if( insertState )
                    event.keyDown.controlKeyState |= kbInsState;

                if( event.keyDown.keyCode != kbNoKey )
                    return True;
                }
            else if( irBuffer.EventType == WINDOW_BUFFER_SIZE_EVENT )
                {
                event.what = evCommand;
                event.message.command = cmScreenChanged;
                event.message.infoPtr = 0;
                return True;
                }
            }
        } while( !pendingEvent );

    return False;
}

void THardwareInfo::waitForEvents( int timeoutMs )
{
    if (!pendingEvent)
        WaitForSingleObject( consoleHandle[cnInput], timeoutMs < 0 ? INFINITE : timeoutMs );
}

void THardwareInfo::interruptEventWait()
{
    // Не реализовано. Потребовалось бы только для многопоточного приложения
    // на Borland C++.
}

BOOL THardwareInfo::setClipboardText( TStringView text )
{
    BOOL result = False;
    if( pOpenClipboard && pOpenClipboard( 0 ) )
        {
        HGLOBAL hData = NULL;
        char *pData;
        if( pEmptyClipboard() &&
            (result = text.empty()) == 0 &&
            (hData = GlobalAlloc( GMEM_MOVEABLE, text.size() + 1 )) != 0 &&
            (pData = (char *) GlobalLock( hData )) != 0
          )
            {
            memcpy(pData, text.data(), text.size());
            pData[text.size()] = '\0';
            GlobalUnlock( hData );
            result = pSetClipboardData( CF_OEMTEXT, hData ) != 0;
            }
        pCloseClipboard();
        if( hData && !result )
            GlobalFree( hData );
        }
    return result;
}

BOOL THardwareInfo::requestClipboardText( void (&accept)( TStringView ) )
{
    BOOL result = False;
    if( pOpenClipboard && pOpenClipboard( 0 ) )
        {
        HGLOBAL hData;
        char *pData;
        if( (hData = pGetClipboardData( CF_OEMTEXT )) != 0 &&
            (result = ((pData = (char *) GlobalLock( hData )) != 0)) == True
          )
            {
            accept( pData );
            GlobalUnlock( hData );
            }
        pCloseClipboard();
        }
    return result;
}

uint32_t THardwareInfo::getTickCountMs()
{
    return GetTickCount();
}

#endif  // __BORLANDC__

uint32_t THardwareInfo::getTickCount() noexcept
{
    // Перевод миллисекунд в такты часов: X мс × 1 с/1000 мс × 18,2 такта/с ≈
    // X/55 тактов.
    return getTickCountMs() / 55;
}

BOOL __stdcall THardwareInfo::ctrlBreakHandler( DWORD dwCtrlType ) noexcept
{
    if( dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT )
        {
        TSystemError::ctrlBreakHit = True;
        return TRUE;
        }
    else
        return FALSE; // Не обрабатываем события CLOSE, LOGOFF и SHUTDOWN.
}

#endif  // __FLAT__
