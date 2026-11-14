/*------------------------------------------------------------*/
/* файл — ttprvlns.cpp */
/*                                                            */
/* функции:  */
/* Метод TTerminal::prevLines. */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TTerminal
#include <tvision/tv.h>

static Boolean findLfBackwards( const char *buffer, ushort &pos, ushort count )
// Предусловие: count >= 1. Постусловие: pos указывает на последний
// проверенный символ.
{
    ++pos;
    do
    {
        if (buffer[--pos] == '\n')
            return True;
    } while (--count > 0);
    return False;
}

ushort TTerminal::prevLines( ushort pos, ushort lines )
{
    if (lines > 0 && pos != queBack)
    {
        do
        {
            if (pos == queBack)
                return queBack;
            bufDec(pos);
            ushort count = (pos >= queBack ? pos - queBack : pos) + 1;
            if (findLfBackwards(buffer, pos, count))
                --lines;
        } while (lines > 0);
        bufInc(pos);
    }
    return pos;
}
