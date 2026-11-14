/*-------------------------------------------------------------------*/
/* файл — mapcolor.cpp */
/*                                                                   */
/* функции:  */
/* mapColor сопоставляет цвет указателю в текущей палитре. */
/*                      palette.                                     */
/*-------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TView
#define Uses_TGroup
#include <tvision/tv.h>

TColorAttr TView::mapColor( uchar index )
{
    TPalette& p = getPalette();
    TColorAttr color;
    if( p[0] != 0 )
        {
        if( 0 < index && index <= p[0] )
            color = p[index];
        else
            return errorAttr;
        }
    else
        color = index;
    if( color == 0 )
        return errorAttr;
    if( owner )
        return owner->mapColor(color);
    return color;
}
