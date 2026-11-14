/*
 * MMENU.CPP. Модуль содержит реализацию класса TMultiMenu.
 *
 *
 *
 */

#define Uses_TEvent
#define Uses_TMenu
#define Uses_TSubMenu
#define Uses_TMenuItem
#define Uses_TMenuBar
#include <tv.h>

#if !defined( __MMENU_H )
#include "mmenu.h"
#endif


/*
 * Глобальный operator+. Объекты всегда находятся в связанном списке, а
 * operator+ вычисляется слева направо. Поэтому добавляем menuItem2 в
 * конец menuItem1 и возвращаем menuItem1.
 *
 *
 */

#if 0 // Эта функция уже предоставляется библиотекой.
TMenuItem& operator +( TMenuItem& menuItem1, TMenuItem& menuItem2 )
{
    TMenuItem *p = &menuItem1;
    while( p->next != NULL )
        p = p->next;
    p->next = &menuItem2;
    return menuItem1;
}
#endif


/*
 * Класс TTestList. TMultiMenu::TMultiMenu — конструктор объекта
 * TMultiMenu. Эта версия принимает массив указателей TMenu.
 *
 *
 *
 *
 *
 */

TMultiMenu::TMultiMenu( const TRect& bounds, TMenu *aMenu[],
            int nMenus ) :    TMenuBar( bounds, aMenu[0] )
{
    if( nMenus == 0)
        for( nMenus = 0; aMenu[nMenus] != NULL; nMenus++ )
            ;
    mList = new TMenu *[nMenus];
    numMenus = nMenus;

    for( int i = 0; i < nMenus; i++ )
        mList[i] = aMenu[i];
}


/*
 * TMultiMenu::TMultiMenu — конструктор объекта TMultiMenu. Эта версия
 * принимает массив объектов TSubMenu.
 *
 */

TMultiMenu::TMultiMenu( const TRect& bounds, TSubMenu aMenu[],
                        int nMenus ) :
    TMenuBar( bounds, aMenu[0] ),
    mList( new TMenu *[nMenus] ),
    numMenus( nMenus )
{
    mList[0] = menu;                  // Первое меню уже создано.
    for( int i = 1; i < nMenus; i++ )
        mList[i] = new TMenu( aMenu[i] );
}


/*
 * TMultiMenu::~TMultiMenu — деструктор объекта TMultiMenu. Уничтожает
 * все сохранённые меню, кроме текущего, которое удалит ~TMenuBar, и
 * освобождает память списка.
 *
 */

TMultiMenu::~TMultiMenu()
{
    for( int i = 0; i < numMenus; i++ )
        if( mList[i] != menu )          // Удаляем все меню, кроме текущего.
            delete mList[i];

    delete [] mList;
}


/*
 * TMultiMenu::handleEvent обрабатывает широковещательное сообщение
 * cmMMChangeMenu. Данные сообщения задают номер меню для переключения и
 * передаются в поле infoInt объекта TEvent.
 *
 */

void TMultiMenu::handleEvent( TEvent& event )
{
    if( event.what == evBroadcast &&
        event.message.command == cmMMChangeMenu )
    {
        if( event.message.infoInt >= 0 &&
            event.message.infoInt < numMenus )
        {
            if( menu != mList[ event.message.infoInt ] )
            {
                menu = mList[ event.message.infoInt ];
                drawView();
            }
        }
        clearEvent( event );
    }
    else
        TMenuBar::handleEvent( event );
}
