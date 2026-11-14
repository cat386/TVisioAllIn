/*-------------------------------------------------------*/
/*                                                       */
/* Демонстрация форм Turbo Vision. */
/*                                                       */
/* Forms.cpp — вспомогательный исходный файл примера TVFORMS. */
/*-------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TKeys
#define Uses_TEvent
#define Uses_TRect
#define Uses_TDialog
#define Uses_TStreamableClass
#define Uses_MsgBox
#include <tvision/tv.h>
__link( RDialog )
__link( RView )

#if !defined( __FORMS_H )
#include "forms.h"
#endif  // __FORMS_H

#if !defined( __LISTDLG_H )
#include "listdlg.h"
#endif  // __LISTDLG_H

#if !defined( __FORMCMDS_H )
#include "formcmds.h"
#endif  // __FORMCMDS_H

#if !defined( __STRING_H )
#include <string.h>
#endif  // __STRING_H

// Сравнивает два буфера; возвращает True при равенстве содержимого.

Boolean compBlocks( void *buf1, void *buf2, ushort bufSize )
{
    return Boolean(memcmp( buf1, buf2, bufSize ) == 0);
}

const char * const TForm::name = "TForm";

void TForm::write( opstream& os )
{

    TDialog::write( os );
    os << keyWidth;

}

void *TForm::read( ipstream& is )
{

    TDialog::read( is );
    is >> keyWidth;
    return this;
}

TStreamable *TForm::build()
{
    return new TForm( streamableInit );
}


TStreamableClass RForm( TForm::name,
                        TForm::build,
                        __DELTA(TForm)
                      );

TForm::TForm( const TRect& bounds, const char *aTitle) :
       TWindowInit(&TForm::initFrame),
       TDialog(bounds, aTitle)
{
}

Boolean TForm::changed()
{
    void *curData;
    ushort compSize;
    Boolean newForm, result;

    compSize = dataSize();
    curData = new char[compSize];
    getData(curData);
    if (prevData == NULL)
        newForm = True;
    else
        newForm = False;

    if (newForm)
        {
        // Создаём пустую запись для сравнения.
        prevData = new char[compSize];
        memset(prevData, 0, compSize);
        }
    if (compBlocks(prevData, curData, compSize))
        result = False;
    else
        result = True;

    delete[] (char *) curData;
    if (newForm)
        {
        delete[] (char *) prevData;
        prevData = NULL;
        }
    if (result)
        return True;
    else
        return False;
}

void TForm::handleEvent( TEvent& event)
{
    // Обрабатываем кнопку CANCEL и клавишу Esc.
    if ( ( (event.what == evKeyDown) && (event.keyDown.keyCode == kbEsc) ) ||
       ( (event.what == evCommand) && (event.message.command == cmCancel) ) )
        {
        clearEvent(event);
        destroy(this);
        return;
        }

    // Обрабатываем кнопку SAVE.
    if ( (event.what == evCommand) && (event.message.command == cmFormSave) )
        {
        clearEvent(event);
        if (changed() == True)
            {
            if (((TListDialog *)listDialog)->saveForm(this))
                {
                destroy(this);
                return;
                }
            }
        else
            {
            destroy(this);                        // Без изменений.
            return;
            }
        }

    TDialog::handleEvent(event);

    // Обрабатываем сообщения TopForm.
    if (event.what == evBroadcast)
        {
        if (event.message.command == cmEditingForm)
            {
            // Переданная широковещательно форма уже редактируется?
            if ( (prevData != NULL) && (event.message.infoPtr == prevData) )
                clearEvent(event);
            }
        else
            // Принадлежит отправившему сообщение ListDialog?
            if (listDialog == event.message.infoPtr)
            {
                if (event.message.command == cmTopForm)
                    clearEvent(event);
                else if (event.message.command == cmCanCloseForm)
                    {
                    if (!valid(cmClose))
                        clearEvent(event);
                    }
                else if (event.message.command == cmCloseForm)
                    destroy(this);
           }
        }
}

Boolean TForm::valid(ushort command)
{
    ushort action;

    action = cmYes;                    // Предполагается вызов унаследованного метода.
    if (command == cmClose)
        {
        if (changed())
            {
            select();
            action = messageBox("Form data has been modified. Save? ",
                                mfYesNoCancel);
            switch (action)
                {
                case cmYes:
                // Пытаемся сохранить изменения; при ошибке отменяем операцию.
                    if (!((TListDialog *)listDialog)->saveForm(this))
                        action = cmCancel;
                    break;
                case cmNo:
                    break;                // Отказываемся от изменений.
                default :
                    action = cmCancel;            // Отменяем запрос закрытия.
                }
            }
        else
            action = cmNo;                        // Изменений нет.
        }
    if (action == cmYes)
        return TDialog::valid(command);
    else
        {
        if (action != cmCancel)
            return True;
        else
            return False;

        }
}
