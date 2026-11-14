/*------------------------------------------------------------*/
/* файл — misc.cpp */
/*                                                            */
/* функции:  */
/* message отправляет сообщение объекту. */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TView
#define Uses_TEvent
#define Uses_TObject
#define Uses_TVMemMgr
#include <tvision/tv.h>
#include <stddef.h>
#include <string.h>

void *message( TView *receiver, ushort what, ushort command, void *infoPtr)
{
    if( receiver == 0 )
        return 0;

    TEvent event;
    event.what = what;
    event.message.command = command;
    event.message.infoPtr = infoPtr;
    receiver->handleEvent( event );
    if( event.what == evNothing )
        return event.message.infoPtr;
    else
        return 0;
}

Boolean lowMemory() noexcept
{
    return Boolean(TVMemMgr::safetyPoolExhausted());
}

size_t strnzcpy( char *dst, TStringView src, size_t dstSize ) noexcept
{
    // Аналог strlcpy. dstSize — размер dst, который всегда завершается
    // нулём, кроме dstSize = 0. Возвращает длину dst без нулевого символа.
    // dst и src не должны перекрываться.
    //
    if (dstSize != 0)
    {
        size_t copyLen = src.size();
        if (copyLen > dstSize - 1)
            copyLen = dstSize - 1;
        memcpy(dst, src.data(), copyLen);
        dst[copyLen] = '\0';
        return copyLen;
    }
    return 0;
}

size_t strnzcat( char *dst, TStringView src, size_t dstSize ) noexcept
{
    // Аналог strlcat, но dst всегда завершается нулём, кроме dstSize = 0.
    // Возвращает длину dst без нулевого символа. dst и src не должны
    // перекрываться.
    //
    if (dstSize != 0)
    {
        size_t dstLen = strlen(dst);
        size_t copyLen = src.size();
        if (copyLen > dstSize - 1 - dstLen)
            copyLen = dstSize - 1 - dstLen;
        memcpy(&dst[dstLen], src.data(), copyLen);
        dst[dstLen + copyLen] = '\0';
        return dstLen + copyLen;
    }
    return 0;
}
