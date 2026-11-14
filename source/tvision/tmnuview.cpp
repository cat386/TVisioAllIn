/*------------------------------------------------------------*/
/* файл — tmnuview.cpp */
/*                                                            */
/* функции:  */
/* Методы TMenuView */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TMenuItem
#define Uses_TMenu
#define Uses_TMenuView
#define Uses_TKeys
#define Uses_TRect
#define Uses_TEvent
#define Uses_TGroup
#define Uses_TMenuBox
#define Uses_TText
#define Uses_opstream
#define Uses_ipstream
#include <tvision/tv.h>

#if !defined( __ASSERT_H )
#include <assert.h>
#endif  // __ASSERT_H

#if !defined( __CTYPE_H )
#include <ctype.h>
#endif  // __CTYPE_H

#if !defined( __STRING_H )
#include <string.h>
#endif  // __STRING_H

#define cpMenuView "\x02\x03\x04\x05\x06\x07"

TMenuItem::TMenuItem(   TStringView aName,
                        ushort aCommand,
                        TKey aKey,
                        ushort aHelpCtx,
                        TStringView p,
                        TMenuItem *aNext
                    ) noexcept
{
    name = newStr( aName );
    command = aCommand;
    disabled = Boolean(!TView::commandEnabled(command));
    keyCode = aKey;
    helpCtx = aHelpCtx;
    if( p.empty() )
        param = 0;
    else
        param = newStr( p );
    next = aNext;
}

TMenuItem::TMenuItem( TStringView aName,
                      TKey aKey,
                      TMenu *aSubMenu,
                      ushort aHelpCtx,
                      TMenuItem *aNext
                    ) noexcept
{
    name = newStr( aName );
    command = 0;
    disabled = Boolean(!TView::commandEnabled(command));
    keyCode = aKey;
    helpCtx = aHelpCtx;
    subMenu = aSubMenu;
    next = aNext;
}

TMenuItem::~TMenuItem()
{
    delete[] (char *)name;
    if( command == 0 )
        delete subMenu;
    else
        delete[] (char *)param;
}

TMenu::~TMenu()
{
    while( items != 0 )
        {
        TMenuItem *temp = items;
        items = items->next;
        delete temp;
        }
}

void TMenuView::trackMouse( TEvent& e, Boolean& mouseActive )
{
    TPoint mouse = makeLocal( e.mouse.where );
    for( current = menu->items; current != 0; current = current->next )
        {
        TRect r = getItemRect( current );
        if( r.contains(mouse) )
            {
            mouseActive = True;
            return;
            }
        }
}

void TMenuView::nextItem()
{
    if( (current = current->next) == 0 )
        current = menu->items;
}

void TMenuView::prevItem()
{
    TMenuItem *p;

    if( (p = current) == menu->items)
        p = 0;

    do  {
        nextItem();
        } while( current->next != p );
}

void TMenuView::trackKey( Boolean findNext )
{
    if( current == 0 )
        {
        current = menu->items;
        if( !findNext )
            prevItem();
        if( current->name != 0 )
            return;
        }

    do  {
        if( findNext )
            nextItem();
        else
            prevItem();
        } while( current->name == 0 );
}

Boolean TMenuView::mouseInOwner( TEvent& e )
{
    if( parentMenu == 0 )
        return False;
    else
        {
        TPoint mouse = parentMenu->makeLocal( e.mouse.where );
        TRect r = parentMenu->getItemRect( parentMenu->current );
        return r.contains( mouse );
        }
}

Boolean TMenuView::mouseInMenus( TEvent& e )
{
    TMenuView *p =  parentMenu;
    while( p != 0 && !p->mouseInView(e.mouse.where) )
        p = p->parentMenu;

    return Boolean( p != 0 );
}

TMenuView *TMenuView::topMenu()
{
    TMenuView *p = this;
    while( p->parentMenu != 0 )
        p = p->parentMenu;
    return p;
}

enum menuAction { doNothing, doSelect, doReturn };

ushort TMenuView::execute()
{
    Boolean    autoSelect = False;
    Boolean    firstEvent = True;
    menuAction action;
    ushort result = 0;
    TMenuItem *itemShown = 0;
    TMenuItem *p;
    TMenuView *target;
    TMenuItem *lastTargetItem = 0;
    TRect  r;
    TEvent e;
    Boolean mouseActive;

    current = menu->deflt;

    mouseActive = False;
    do  {
        action = doNothing;
        getEvent(e);
        switch (e.what)
            {
            case  evMouseDown:
                if( mouseInView(e.mouse.where) || mouseInOwner(e) )
                    {
                    trackMouse(e, mouseActive);
                    // autoSelect позволяет открыть выбранное подменю непосредственно при
                    // MouseDown. Этого нельзя делать, если подменю только что закрыли
                    // щелчком по имени либо это не строка меню.
                    //
                    if( size.y == 1 )
                        autoSelect = (Boolean) (!current || lastTargetItem != current);
                    // MouseDown в родительском меню закрывает подменю, кроме только что
                    // открытого.
                    else if( !firstEvent && mouseInOwner(e) )
                        action = doReturn;
                    }
                else
                    {
                    // Щелчок вне меню закрывает его. Передаём событие представлению,
                    // восстанавливающему фокус.
                    if( putClickEventOnExit )
                        putEvent(e);
                    action =  doReturn;
                    }
                break;
            case  evMouseUp:
                trackMouse(e, mouseActive);
                if( mouseInOwner(e) )
                    current = menu->deflt;
                else if( current != 0 )
                    {
                    if( current->name != 0 )
                        {
                        if ( current != lastTargetItem )
                            action = doSelect;
                        else if ( size.y == 1 )
                        // Если пункт строки меню закрыт, выходим и прекращаем слушать события.
                        //
                            action = doReturn;
                        else
                        // MouseUp не открывает подменю, только что закрытое щелчком по имени.
                        //
                            {
                            action = doNothing;
                            // Но следующее откроет.
                            lastTargetItem = 0;
                            }
                        }
                    }
                else if ( mouseActive && !mouseInView(e.mouse.where) )
                    action = doReturn;
                else if ( size.y != 1 )
                // Если MouseUp внутри меню, но не на выделяемом пункте, например на поле
                // или разделителе, автоматически выделяется пункт по умолчанию либо
                // первый. Это добавлено в Turbo Vision 2.0, но не имеет смысла для
                // строки меню, где прежде тоже действовало.
                    {
                    current = menu->deflt;
                    if (current == 0)
                        current = menu->items;
                    action = doNothing;
                    }
                break;
            case  evMouseMove:
                if( e.mouse.buttons != 0 )
                    {
                    trackMouse(e, mouseActive);
                    if( !(mouseInView(e.mouse.where) || mouseInOwner(e)) &&
                        mouseInMenus(e) )
                        action = doReturn;
                    // Закрытый щелчком по имени пункт строки меню остаётся выделенным до
                    // MouseUp. Если затем перетаскиванием выбрать другой пункт, тот
                    // откроется автоматически.
                    else if ( size.y == 1 && mouseActive && current != lastTargetItem )
                        autoSelect = True;
                    }
                break;
            case  evKeyDown:
                switch( e.keyDown.keyCode )
                    {
                    case  kbUp:
                    case  kbDown:
                        if( size.y != 1 )
                            trackKey(Boolean(e.keyDown.keyCode == kbDown));
                        else if( e.keyDown.keyCode == kbDown )
                            autoSelect =  True;
                        break;
                    case  kbLeft:
                    case  kbRight:
                        if( size.y == 1 )
                            trackKey(Boolean(e.keyDown.keyCode == kbRight));
                        else if( parentMenu != 0 )
                            action = doReturn;
                        break;
                    case  kbHome:
                    case  kbEnd:
                        if( size.y != 1 )
                            {
                            current = menu->items;
                            if( e.keyDown.keyCode == kbEnd )
                                trackKey(False);
                            }
                        break;
                    case  kbEnter:
                        if( size.y == 1 )
                            autoSelect =  True;
                        action = doSelect;
                        break;
                    case  kbEsc:
                        action = doReturn;
                        if( parentMenu == 0 || parentMenu->size.y != 1 )
                            clearEvent(e);
                        break;
                    default:
                        target = this;
                        if( !getAltCharStr(e.keyDown).empty() )
                            {
                            target = topMenu();
                            p = target->findAltShortcut(e.keyDown);
                            }
                        else
                            p = findItem(e.keyDown.getText());
                        if( p == 0 )
                            {
                            p = topMenu()->hotKey(e.keyDown);
                            if( p != 0 && commandEnabled(p->command) )
                                {
                                result = p->command;
                                action = doReturn;
                                }
                            }
                        else if( target == this )
                            {
                            if( size.y == 1 )
                                autoSelect = True;
                            action = doSelect;
                            current = p;
                            }
                        else if( parentMenu != target ||
                                 parentMenu->current != p )
                            action = doReturn;
                    }
                break;
            case  evCommand:
                if( e.message.command == cmMenu )
                    {
                    autoSelect = False;
                    lastTargetItem = 0;
                    if (parentMenu != 0 )
                        action = doReturn;
                    }
                else
                    action = doReturn;
                break;
            }

        // Если подменю закрыто щелчком по имени, а мышь перетащена на другой
        // пункт, подменю откроется при следующем наведении.
        //
        if( lastTargetItem != current )
            lastTargetItem = 0;

        if( itemShown != current )
            {
            itemShown =  current;
            drawView();
            }

        if( (action == doSelect || (action == doNothing && autoSelect)) &&
            current != 0 &&
            current->name != 0 )
                {
                if( current->command == 0 && !current->disabled )
                    {
                    if( (e.what & (evMouseDown | evMouseMove)) != 0 )
                        putEvent(e);
                    r = getItemRect( current );
                    r.a.x = r.a.x + origin.x;
                    r.a.y = r.b.y + origin.y;
                    r.b = owner->size;
                    if( size.y == 1 )
                        r.a.x--;
                    target = topMenu()->newSubView(r, current->subMenu, this);
                    result = owner->execView(target);
                    destroy( target );
                    lastTargetItem = current;
                    menu->deflt = current;
                    }
                else if( action == doSelect )
                    result = current->command;
                }

        if( result != 0 && commandEnabled(result) )
            {
            action =  doReturn;
            clearEvent(e);
            }
        else
            result = 0;

        firstEvent = False;
        } while( action != doReturn );

    if( e.what != evNothing &&
        (parentMenu != 0 || e.what == evCommand))
            putEvent(e);
    if( current != 0 )
        {
        menu->deflt = current;
        current = 0;
        drawView();
        }
    return result;
}

TMenuItem *TMenuView::findItem( char ch )
{
    return findItem(TStringView(&ch, 1));
}

TMenuItem *TMenuView::findItem( TStringView ch )
{
    TMenuItem *p = menu->items;
    while( p != 0 )
        {
        TStringView shortcut = hotKeyStr(p->name);
        if( !p->disabled &&
            !shortcut.empty() &&
            TText::equalsIgnoreCase(ch, hotKeyStr(p->name))
          )
            return p;
        p =  p->next;
        }
    return 0;
}

TMenuItem *TMenuView::findAltShortcut( const KeyDownEvent &keyDown )
{
    TMenuItem *p = 0;
    // Сначала ищем пункт, соответствующий тексту KeyDownEvent.
    if( !keyDown.getText().empty() )
        p = findItem( getAltCharStr(keyDown) );
    // Если не найден, ищем по коду клавиши KeyDownEvent.
    if( p == 0 )
        p = findItem( getAltChar(keyDown.keyCode) );
    return p;
}

TRect TMenuView::getItemRect( TMenuItem * )
{
    return TRect( 0, 0, 0, 0 );
}

ushort TMenuView::getHelpCtx()
{
    TMenuView *c = this;

    while( c != 0 &&
                (c->current == 0 ||
                 c->current->helpCtx == hcNoContext ||
                 c->current->name == 0 )
         )
        c = c->parentMenu;

    if( c != 0 )
        return c->current->helpCtx;
    else
        return helpCtx;
}

TPalette& TMenuView::getPalette() const
{
    static TPalette palette( cpMenuView, sizeof( cpMenuView )-1 );
    return palette;
}

Boolean TMenuView::updateMenu( TMenu *menu )
{
    Boolean res = False;
    if( menu != 0 )
        {
        for( TMenuItem *p = menu->items; p != 0; p = p->next )
            {
            if( p->name != 0 )
                {
                if( p->command == 0 )
                    {
                    if( updateMenu(p->subMenu) == True )
                        res = True;
                    }
                else
                    {
                    Boolean commandState = commandEnabled(p->command);
                    if( p->disabled == commandState )
                        {
                        p->disabled = Boolean(!commandState);
                        res = True;
                        }
                    }
                }
            }
        }
    return res;
}

void TMenuView::do_a_select( TEvent& event )
{
    putEvent( event );
    event.message.command = owner->execView(this);
    if( event.message.command != 0 && commandEnabled(event.message.command) )
        {
        event.what = evCommand;
        event.message.infoPtr = 0;
        putEvent(event);
        }
    clearEvent(event);
}

void TMenuView::handleEvent( TEvent& event )
{
    if( menu != 0 )
        switch (event.what)
            {
            case  evMouseDown:
                do_a_select(event);
                break;
            case  evKeyDown:
                if( findAltShortcut(event.keyDown) != 0 )
                    do_a_select(event);
                else
                    {
                    TMenuItem *p = hotKey(event.keyDown);
                    if( p != 0 && commandEnabled(p->command))
                        {
                        event.what = evCommand;
                        event.message.command = p->command;
                        event.message.infoPtr = 0;
                        putEvent(event);
                        clearEvent(event);
                        }
                    }
                break;
            case  evCommand:
                if( event.message.command == cmMenu )
                    do_a_select(event);
                break;
            case  evBroadcast:
                if( event.message.command == cmCommandSetChanged )
                    {
                    if( updateMenu(menu) )
                        drawView();
                    }
                break;
            }
}


TMenuItem *TMenuView::findHotKey( TMenuItem *p, TKey key )
{

    while( p != 0 )
        {
        if( p->name != 0 )
            {
            if( p->command == 0 )
                {
                TMenuItem *T;
                if( (T = findHotKey( p->subMenu->items, key )) != 0 )
                    return T;
                }
            else if( !p->disabled &&
                     p->keyCode != kbNoKey &&
                     p->keyCode == key
                   )
                return p;
            }
        p =  p->next;
        }
    return 0;
}

TMenuItem *TMenuView::hotKey( TKey key )
{
    return findHotKey( menu->items, key );
}

TMenuView *TMenuView::newSubView( const TRect& bounds,
                                  TMenu *aMenu,
                                  TMenuView *aParentMenu
                               )
{
    return new TMenuBox( bounds, aMenu, aParentMenu );
}

#if !defined(NO_STREAMABLE)

void TMenuView::writeMenu( opstream& os, TMenu *menu )
{
    uchar tok = 0xFF;

    assert( menu != 0 );

    for( TMenuItem *item = menu->items; item != 0; item = item->next )
        {
        os << tok;
        os.writeString( item->name );
        os << item->command << (int)(item->disabled)
           << item->keyCode << item->helpCtx;
        if( item->name != 0 )
            {
            if( item->command == 0 )
                writeMenu( os, item->subMenu );
            else
                os.writeString( item->param );
            }
        }

    tok = 0;
    os << tok;
}

void TMenuView::write( opstream& os )
{
    TView::write( os );
    writeMenu( os, menu );
}

TMenu *TMenuView::readMenu( ipstream& is )
{
    TMenu *menu = new TMenu;
    TMenuItem **last = &(menu->items);
    TMenuItem *item;

    uchar tok;
    is >> tok;

    while( tok != 0 )
        {
        assert( tok == 0xFF );
        item = new TMenuItem( 0, 0, (TMenu *)0 );
        *last = item;
        last = &(item->next);
        item->name = is.readString();
        int temp;
        is >> item->command >> temp
           >> item->keyCode >> item->helpCtx;
        item->disabled = Boolean( temp );
        if( item->name != 0 )
            {
            if( item->command == 0 )
                item->subMenu = readMenu( is );
            else
                item->param = is.readString();
            }
        is >> tok;
        }
    *last = 0;
    menu->deflt = menu->items;
    return menu;
}

void *TMenuView::read( ipstream& is )
{
    TView::read( is );
    menu = readMenu( is );
    parentMenu = 0;
    current = 0;
    return this;
}

TStreamable *TMenuView::build()
{
    return new TMenuView( streamableInit );
}

TMenuView::TMenuView( StreamableInit ) noexcept : TView( streamableInit )
{
}


#endif
