/*------------------------------------------------------------------------*/
/*                                                                        */
/* файл — drivers2.cpp */
/*                                                                        */
/* функции:  */
/* ctrlToArrow сопоставляет управляющие клавиши стрелкам. */
/* cstrlen вычисляет длину строки с управляющими маркерами. */
/*                                                                        */
/*------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TKeys
#define Uses_TText
#include <tvision/tv.h>

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  ctrlToArrow                                                           */
/*                                                                        */
/* Аргумент: */
/*                                                                        */
/* keyCode — скан-код для сопоставления со стрелкой цифровой клавиатуры. */
/*                                                                        */
/* Возвращает: */
/*                                                                        */
/* Скан-код стрелки, соответствующей клавише WordStar, */
/* либо исходный код, если соответствия нет. */
/*                                                                        */
/*------------------------------------------------------------------------*/
ushort ctrlToArrow(ushort keyCode) noexcept
{

const uchar ctrlCodes[] =
    {
    kbCtrlS, kbCtrlD, kbCtrlE, kbCtrlX, kbCtrlA,
    kbCtrlF, kbCtrlG, kbCtrlV, kbCtrlR, kbCtrlC, kbCtrlH
    };

const ushort arrowCodes[] =
    {
    kbLeft, kbRight, kbUp, kbDown, kbHome,
    kbEnd,  kbDel,   kbIns,kbPgUp, kbPgDn, kbBack
    };

    for( size_t i = 0; i < sizeof(ctrlCodes); i++ )
        if( (keyCode & 0x00ff) == ctrlCodes[i] )
            return arrowCodes[i];
    return keyCode;
}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  cstrlen                                                               */
/*                                                                        */
/* Аргумент: */
/*                                                                        */
/* text — входная строка. */
/*                                                                        */
/* Возвращает: */
/*                                                                        */
/* Видимую длину строки без учёта символов '~'. */
/*                                                                        */
/* Примечания: */
/*                                                                        */
/* Используется для определения видимой длины строк команд, */
/* где '~' переключает атрибуты отображения. */
/*                                                                        */
/*------------------------------------------------------------------------*/

int cstrlen( TStringView text ) noexcept
{
#ifdef __BORLANDC__
    const char _FAR *limit = &text[text.size()];
    const char _FAR *s = &text[0];
    int len = 0;
    while( s < limit )
        {
        if( *s++ != '~' )
            len++;
        }
    return len;
#else
    size_t i = 0, width = 0;
    while (i < text.size())
    {
        if (text[i] != '~')
            TText::next(text, i, width);
        else
            ++i;
    }
    return width;
#endif
}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  strwidth                                                              */
/*                                                                        */
/* Аргумент: */
/*                                                                        */
/* text — входная строка. */
/*                                                                        */
/* Возвращает: */
/*                                                                        */
/* Видимая длина строки. */
/*                                                                        */
/*------------------------------------------------------------------------*/

int strwidth( TStringView text ) noexcept
{
    return TText::width(text);
}
