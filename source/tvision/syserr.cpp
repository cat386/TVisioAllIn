/*------------------------------------------------------------*/
/* файл — syserr.cpp */
/*                                                            */
/* функции:  */
/* Методы TSystemError */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TDrawBuffer
#define Uses_TSystemError
#define Uses_TEvent
#define Uses_TScreen
#define Uses_THardwareInfo
#include <tvision/tv.h>

#if !defined( __DOS_H )
#include <dos.h>
#endif  // __DOS_H

#if !defined( __STDIO_H )
#include <stdio.h>
#endif  // __STDIO_H

#if !defined( __STRING_H )
#include <string.h>
#endif  // __STRING_H

#if !defined( __MATH_H )
#include <math.h>
#endif  // __MATH_H

Boolean _NEAR TSystemError::ctrlBreakHit = False;
Boolean _NEAR TSystemError::saveCtrlBreak = False;

#if !defined( __FLAT__ )
short ( _FAR * _NEAR TSystemError::sysErrorFunc )(short,uchar) = &TSystemError::sysErr;
ushort _NEAR TSystemError::sysColorAttr = 0x4E4F;
ushort _NEAR TSystemError::sysMonoAttr = 0x7070;
Boolean _NEAR TSystemError::sysErrActive = False;
Boolean _NEAR TSystemError::inIDE = False;

TPMRegs TSystemError::Int24Regs;
void (interrupt far *TSystemError::Int24RMThunk)();
void (interrupt far *TSystemError::Int24RMCallback)();
unsigned TSystemError::Int24RMThunkSel;

const SecretWord = 1495;
const productID  =  136;

static void checkIDE()
{
    Int11trap trap;

    _BX = SecretWord;
    _AX = SecretWord;

    _genInt(0x12);
}
#endif

TSystemError::TSystemError() noexcept
{
#if !defined(__FLAT__)
    inIDE = False;
    checkIDE();

    if( THardwareInfo::getDPMIFlag() )
        setupDPMI();
#endif
    resume();
}

TSystemError::~TSystemError()
{
    suspend();
#if !defined( __FLAT__ )
    if( THardwareInfo::getDPMIFlag() )
        shutdownDPMI();
#endif
}

#if defined( __FLAT__ )             // 16-битная версия находится в SYSINT.ASM.
void TSystemError::resume() noexcept
{
    THardwareInfo::setCtrlBrkHandler( TRUE );
}

void TSystemError::suspend() noexcept
{
    THardwareInfo::setCtrlBrkHandler( FALSE );
}
#endif

#if !defined( __FLAT__ )
ushort TSystemError::selectKey()
{
    TEvent key;
    ushort crsrType = TScreen::getCursorType();

    TScreen::setCursorType( 0x2000 );

    int ch;

    do {
        // Приём пригоден только для 16 бит! Кэширование событий 32-битным кодом
        // приведёт к бесконечному циклу, если событие мыши предшествует нужной
        // клавише. Впрочем, в 32-битной версии этого кода нет.
        //
        key.getKeyEvent();
        if( key.what == evNothing )
            continue;               // Событие клавиатуры ещё не получено.

        ch = key.keyDown.keyCode & 0xFF;
        if( ch == 13 || ch == 27 )  // Выходим из цикла только по Esc или Enter.
            break;
    } while( 1 );

    TScreen::setCursorType( crsrType );
    return ch == 27;
}

short TSystemError::sysErr( short errorCode, uchar drive )
{
    ushort c = ( (TScreen::screenMode & 0x00FF) != TDisplay::smMono  ) ?
                                        sysColorAttr : sysMonoAttr;
    char s[ 63 ];
    TDrawBuffer b;

    /*
 * Документированы 22 ошибки устройств, каждой соответствует строка
 * errorString[]. Эта проверка предотвращает сбой при неожиданном значении.
 */
    if( errorCode < (sizeof(errorString) / sizeof(errorString[0])) )
        sprintf( s, errorString[ errorCode ], drive + 'A' );
    else
        sprintf( s, "Unknown critical error %d on drive %c", errorCode, drive + 'A' );

    b.moveChar( 0, ' ', c, TScreen::screenWidth);
    b.moveCStr( 1, s, c);
    b.moveCStr( TScreen::screenWidth-1-cstrlen(sRetryOrCancel), sRetryOrCancel, c);
    swapStatusLine(b);
    int res = selectKey();
    swapStatusLine(b);
    return res;
}


Int11trap::Int11trap()
{
    oldHandler = getvect( 0x11 );
    setvect( 0x11, &Int11trap::handler );
}

Int11trap::~Int11trap()
{
    setvect( 0x11, oldHandler );
}

void interrupt (_FAR * _NEAR Int11trap::oldHandler)(...) = 0;

void interrupt Int11trap::handler(...)
{
    if( _AX == SecretWord && _BX == productID )
        TSystemError::inIDE = True;
    oldHandler();
}

#endif
