/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   HELP.H                                                                */
/*                                                                         */
/* Определяет классы THelpViewer и THelpWindow. */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined( __HELP_H )
#define __HELP_H

#define Uses_TStreamable
#define Uses_ipstream
#define Uses_opstream
#define Uses_fpstream
#define Uses_TObject
#define Uses_TPoint
#define Uses_TRect
#define Uses_TEvent
#define Uses_TScroller
#define Uses_TScrollBar
#define Uses_TWindow
#include <tvision/tv.h>

#include <tvision/helpbase.h>

/* ---------------------------------------------------------------------- */
/* класс THelpViewer */
/*                                                                        */
/* Структура палитры */
/* 1 = Обычный */
/* 2 = Ключевое слово */
/* 3 = Выделенное ключевое слово */
/* ---------------------------------------------------------------------- */

class THelpViewer : public TScroller
{
public:

    THelpViewer( const TRect&, TScrollBar*, TScrollBar*, THelpFile*, ushort ) noexcept;
    ~THelpViewer();

    virtual void changeBounds( const TRect& );
    virtual void draw();
    virtual TPalette& getPalette() const;
    virtual void handleEvent( TEvent& );
    void makeSelectVisible( int, TPoint&, uchar&, int& );
    void switchToTopic( int );

    THelpFile *hFile;
    THelpTopic *topic;
    int selected;
};

/* ---------------------------------------------------------------------- */
/* класс THelpWindow */
/*                                                                        */
/* Структура палитры */
/* 1 = Неактивная рамка */
/* 2 = Активная рамка */
/* 3 = Значок рамки */
/* 4 = Область страницы полосы прокрутки */
/* 5 = Элементы полосы прокрутки */
/* 6 = Обычный текст справки */
/* 7 = Ключевое слово справки */
/* 8 = Выделенное ключевое слово справки */
/* ---------------------------------------------------------------------- */

class THelpWindow : public TWindow
{

    static const char * _NEAR helpWinTitle;

public:

    THelpWindow( THelpFile*, ushort ) noexcept;

    virtual TPalette& getPalette() const;
};


extern void notAssigned( opstream& s, int value );

extern TCrossRefHandler crossRefHandler;

#endif  // __HELP_H
