/*-------------------------------------------------------*/
/*                                                       */
/* Демонстрация форм Turbo Vision. */
/*                                                       */
/* Listdlg.cpp — вспомогательный исходный файл примера TVFORMS. */
/*-------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TRect
#define Uses_TSortedListBox
#define Uses_TDialog
#define Uses_MsgBox
#define Uses_TProgram
#define Uses_TScrollBar
#define Uses_TButton
#define Uses_TResourceFile
#define Uses_TEvent
#define Uses_TApplication
#define Uses_TKeys
#define Uses_TDeskTop
#define Uses_TLabel
#define Uses_fpstream
#define Uses_TStreamableClass
#include <tvision/tv.h>
__link( RResourceCollection )
__link( RDialog )
__link( RScrollBar )

#if !defined( __LISTDLG_H )
#include "listdlg.h"
#endif  // __LISTDLG_H

#if !defined( __FORMCMDS_H )
#include "formcmds.h"
#endif  // __FORMCMDS_H

#if !defined( __FORMS_H )
#include "forms.h"
#endif  // __FORMS_H

#if !defined( __DATACOLL_H )
#include "datacoll.h"
#endif  // __DATACOLL_H

#if !defined( __STDLIB_H )
#include <stdlib.h>
#endif  // __STDLIB_H

#if !defined( __STDIO_H )
#include <stdio.h>
#endif  // __STDIO_H

#if !defined( __STRING_H )
#include <string.h>
#endif  // __STRING_H

#if !defined( __DIR_H )
#include <dir.h>
#endif  // __DIR_H

Boolean fileExists( char *name )
{
    struct ffblk sr;
    int ccode;

    ccode = findfirst(name, &sr, 0);
    if (!ccode)
        return True;
    else
        return False;
}

// TListKeyBox

TListKeyBox::TListKeyBox(const TRect& bounds, ushort aNumCols,
                  TScrollBar *aScrollBar):
     TSortedListBox(bounds, aNumCols, aScrollBar)
{
}

void *TListKeyBox::getKey( const char *s )
{
    if (list()->keyType == stringKey)
        return (void *) s;
    else
        return 0;
}

void TListKeyBox::getText( char *dest, short item, short maxLen )
{
    switch (list()->keyType)
        {
        case stringKey:
            TSortedListBox::getText(dest, item, maxLen);
            break;
        case longIntKey:
            ltoa(*(int32_t *)list()->keyOf(list()->at(item)), dest, 10);
            break;
        }
}

// TListDialog

TListDialog::TListDialog( char *rezName, char *title) :
    TWindowInit(&TListDialog::initFrame),
    TDialog(TRect( 2, 2, 32, 15 ), title),
    dataCollection(0),
    fileName(newStr(rezName)),
    isValid(False),
    modified(False)
{
    const short
        buttonCt      = 4,
        listX         = 2,
        listY         = 3,
        formWd        = 30,
        formHt        = 13,
        defaultListWd = 12,
        listHt        = buttonCt * 2,
        buttonWd      = 12,
        buttonY       = listY;
    TScrollBar *sb;
    short y;
    TForm *f;
    short listWd;
    short buttonX;

    // Читаем данные из потока ресурсов.
    if (openDataFile(fileName, formDataFile, ios::in) == True)
        {
        // Получаем горизонтальный размер ключевого поля.
        f = (TForm *)formDataFile->get("FormDialog");
        if (f == NULL)
            {
            messageBox("Error accessing file data.", mfError | mfOKButton);
            return;
            }

            // Ширина списка определяется шириной ключа. При необходимости увеличиваем весь диалог.
        if (f->keyWidth > defaultListWd)
            {
            listWd = f->keyWidth;
            growTo((short)(formWd + listWd - defaultListWd), (short)formHt);
            }
        else
            listWd = defaultListWd;

        // Перемещаем в правый верхний угол рабочего стола.
        TRect r (TProgram::deskTop->getExtent());   // Координаты рабочего стола.
        moveTo((short)(r.b.x - size.x), 1);

        destroy(f);

        // Читаем коллекцию данных в память.
        dataCollection = (TDataCollection *)formDataFile->get("FormData");
        if (dataCollection != NULL)
            {
            // Успешно загружено: создаём диалог ListDialog.

            // Полоса прокрутки.
            sb = new TScrollBar( TRect(listX + listWd, listY,
                     listX + listWd + 1, listY + listHt));
            insert(sb);

            // Список.
            list = new TListKeyBox( TRect(listX, listY, listX + listWd,
                       listY + listHt), 1, sb);
            list->newList(dataCollection);
            insert(list);

            // Метка.
            insert(new TLabel ( TRect(listX, listY - 1,
                       listX + 10, listY), "~K~eys", list));

            // Кнопки.
            buttonX = listX + listWd + 2;
            y = buttonY;

            insert(new TButton (TRect(buttonX, y, buttonX + buttonWd,
                       y + 2), "~E~dit", cmFormEdit, bfDefault));

            y += 2;

            insert(new TButton (TRect(buttonX, y, buttonX + buttonWd,
                       y + 2), "~N~ew", cmFormNew, bfNormal));

            y += 2;
            insert(new TButton (TRect(buttonX, y, buttonX + buttonWd,
                       y + 2), "~D~elete", cmFormDel, bfNormal));

            y += 2;

            insert(new TButton (TRect(buttonX, y, buttonX + buttonWd,
                       y + 2), "~S~ave", cmListSave, bfNormal));

            selectNext(False);      // Выбираем первое поле.
            isValid = True;
            }
        }
}

TListDialog::~TListDialog(void)
{
    if (dataCollection)
        destroy(dataCollection);
    if (formDataFile != NULL)
        destroy(formDataFile);
    if (fileName != NULL)
        delete[] fileName;
}

void TListDialog::close(void)
{
    if (valid(cmClose))
        {
        // При большом числе открытых форм приостанавливаем обновление экрана рабочего стола.
        TProgram::deskTop->lock();
        message(TProgram::deskTop, evBroadcast, cmCloseForm, this);
        TProgram::deskTop->unlock();
        destroy(this);
        }
}

TForm *TListDialog::editingForm()
{
    // Возвращаем указатель на форму, редактирующую текущий выбранный элемент.

    return (TForm *)message(TProgram::deskTop, evBroadcast, cmEditingForm,
                            dataCollection->at(list->focused));
}

void TListDialog::formOpen(Boolean newForm)
{
    TForm *f;

    if (!newForm)
        {
        // Коллекция пуста?
        if (dataCollection->getCount() == 0)
            return;

        // Если выбранный элемент редактируется, поднимаем его форму наверх.
        f = editingForm();
        if (f != NULL)
            {
            f->select();
            return;
            }
        }

    // Элемент не редактируется: открываем новую форму из файла ресурсов.
    f = (TForm *) formDataFile->get("FormDialog");
    if (f == NULL)
        messageBox("Error opening form.", mfError | mfOKButton);
    else
        {
        f->listDialog = this;                // Форма ссылается обратно на список.
        if (newForm)
            f->prevData = NULL;               // Добавляем новую форму.
        else
            {
            // Редактируем данные коллекции.
            f->prevData = dataCollection->at(list->focused);
            f->setData(f->prevData);
            }
        if (TApplication::application->validView(f) != NULL)
            {
            stackOnPrev(f);
            TProgram::deskTop->insert(f);      // Вставляем и выбираем.
            }
        }
}

void TListDialog::deleteSelection()
{
    TForm *f;

    // Коллекция пуста?
    if (dataCollection->getCount() == 0)
        return;

    // Запрещаем удалять данные, которые уже редактируются.
    f = editingForm();
    if (f != NULL)
        {
        f->select();
        messageBox("Data is already being edited. Close form before deleting.",
                    mfWarning | mfOKButton);
        return;
        }

    // Подтверждаем удаление.
    if (messageBox("Are you sure you want to delete this item?",
                    mfWarning | mfYesNoCancel) == cmYes)
        {
        dataCollection->atFree(list->focused);
        list->setRange(dataCollection->getCount());
        list->drawView();
        modified = True;
        }
}

void TListDialog::handleEvent(TEvent& event)
{
    if ( (event.what == evKeyDown) && (event.keyDown.keyCode == kbEsc) )
         {
         event.what = evCommand;
         event.message.command = cmClose;
         event.message.infoPtr = 0;
         }

    TDialog::handleEvent(event);

    switch (event.what)
        {
        case evCommand:
            switch (event.message.command)
                {
                case cmFormEdit:
                    formOpen(False);
                    break;
                case cmFormNew:
                    formOpen(True);
                    break;    
                case cmFormDel: 
                    deleteSelection();
                    break;   
                case cmListSave: 
                    if (modified)
                        saveList();
                     break;
                default: 
                    return;
                }
            clearEvent(event);
            break;
        case evKeyDown:
            switch (event.keyDown.keyCode)
                { 
                case kbIns: 
                    formOpen(True);
                    break;
                default: 
                    return;
                }
            clearEvent(event);
            break;
        case evBroadcast:
            switch (event.message.command)
                {  // Обрабатываем широковещательное сообщение от TSortedListBox.
                case cmListItemSelected: 
                    formOpen(False);
                    break;

                // Запрещаем одновременное редактирование файла двумя списками.
                case cmEditingFile:
                    if (strcmp(fileName, (char *)event.message.infoPtr) == 0)
                        clearEvent(event);
                    break;

                // Отвечаем на поиск верхнего диалога списка.
                case cmTopList:
                    clearEvent(event);
                    break;
                }
            break;
        }
}

Boolean TListDialog::openDataFile( char *name,
    TResourceFile *&dataFile, pstream::openmode mode )
{
    fpstream* s;

    s = new fpstream(name, mode);
    dataFile = new TResourceFile(s);
    if (!s->good())
        {
        destroy(dataFile);
        dataFile = NULL;
        return False;
        }
    else
        return True;
}

Boolean TListDialog::saveList()
{
    TResourceFile *newDataFile;
    TForm *form;
    char drive[MAXDRIVE];
    char d[MAXDIR];
    char n[MAXFILE];
    char e[MAXEXT];
    char bufStr[MAXPATH];
    short ccode;

    // Коллекция пуста? Нет изменений?
    if ( (dataCollection->getCount() == 0) || (!modified) )
        {
        return True;
        }

    // saveList = False; читаем определение формы из исходного файла форм.
    //
    form = (TForm *)formDataFile->get("FormDialog");
    if (form == NULL)
        messageBox("Cannot find original file. Data not saved.",
                    mfError | mfOKButton);
    else
        {
        // Создаём новый файл данных.
        fnsplit(fileName, drive, d, n, e);
        fnmerge(bufStr, drive, d, n, ".$$$");
        if (openDataFile(bufStr, newDataFile, ios::out) == False)
            messageBox("Cannot create file. Data not saved.",
                        mfError | mfOKButton);
        else
            {
            // Создаём новый файл по форме и коллекции в памяти.
            newDataFile->put(form, "FormDialog");
            newDataFile->put(dataCollection, "FormData");
            newDataFile->flush();
            destroy(newDataFile);

            // Закрываем исходный файл и переименовываем его в .BAK.
            destroy (formDataFile);
            formDataFile = NULL;
            fnmerge(bufStr, drive, d, n, ".bak");
            if (fileExists(bufStr))
                ::remove(bufStr);
            ccode = rename(fileName, bufStr);
            // Ошибка удаления старого .BAK или переименования исходного файла в .BAK?
            if (ccode)
                {
                messageBox("Cannot create .BAK file. Data not saved.",
                            mfError | mfOKButton);

                // Пытаемся вновь открыть исходный файл. Новые данные остаются в памяти.
                if (openDataFile(fileName, formDataFile, ios::in) == False)
                    {
                    messageBox("Cannot re-open original file.",
                                mfError | mfOKButton);
                    destroy(this);        // Продолжение невозможно. Освобождаем данные и закрываем окно. }
                    }
                }
            else
                {
                // Переименовываем временный файл в исходный и открываем заново.
                fnmerge(bufStr, drive, d, n, ".$$$");
                rename(bufStr, fileName);
                openDataFile(fileName, formDataFile, ios::in);

                modified = False;
                destroy(form);
                return True;
                }
            }
            destroy(form);
        }
    return False;
}

Boolean TListDialog::saveForm(TDialog *f)
{
    ccIndex i;
    void *p;

    // Проверяем данные перед обновлением коллекции.
    if (!f->valid(cmFormSave))
      return False;

    // Получаем данные из формы, не используя резервный пул памяти.
    p = malloc(dataCollection->itemSize);
    if (p == NULL)
        {
        TApplication::application->outOfMemory();
        return False;
        }

    memset(p, 0, dataCollection->itemSize);
    f->getData(p);
    // Если дубликаты запрещены, проверяем, что добавляемый ключ уникален.
    if ( (!(dataCollection->duplicates) && dataCollection->search(dataCollection->keyOf(p), i)) )
        if ( (((TForm*)f)->prevData == NULL) || (((TForm *)f)->prevData != dataCollection->at(i)) )
            {
            free(p);
            messageBox("Duplicate keys are not allowed in this database. "
                       "Delete duplicate record before saving this form.",
                        mfError | mfOKButton);
            return False;
            }

    // Освобождаем прежние данные.
    if (((TForm *)f)->prevData != NULL)
        dataCollection->free(((TForm*)f)->prevData);

    // TDataCollection.Insert может завершиться ошибкой, поскольку не
    // использует резервный пул. После вставки проверяем поле status и при
    // необходимости освобождаем данные.

    dataCollection->insert(p);
    if (dataCollection->status != 0)
        {
        free(p);
        TApplication::application->outOfMemory();
        return False;
        }

    // Успешно: сохраняем указатель на исходные данные.
    ((TForm *)f)->prevData = p;

    // Перерисовываем список.
    list->setRange(dataCollection->getCount());
    list->drawView();

    modified = True;
    return True;
}

void TListDialog::stackOnPrev(TDialog *f)
{
    TForm *topForm;

    // Располагаем над верхней формой либо над списком, если форма первая.
    topForm = (TForm *)message(owner, evBroadcast, cmTopForm, this);
    if (topForm != NULL)
    // Располагаем над предыдущей верхней формой.
        f->moveTo(topForm->origin.x + 1, topForm->origin.y + 1);
    else
        {
        // Располагаем справа или слева от ListDialog.
        if (origin.x > f->size.x)
            f->moveTo(0, origin.y);
        else
            f->moveTo(origin.x + size.x + 1, origin.y);
        }

    // Видна на рабочем столе? Обеспечиваем видимость хотя бы половины формы.
    TRect r = owner->getExtent();                      // Координаты рабочего стола.
    if (f->origin.x + size.x / 2 > r.b.x) 
        f->moveTo(0, 1);
    if (f->origin.y + size.y / 2 > r.b.y)
        f->moveTo(f->origin.x, 1);
}

Boolean TListDialog::valid(ushort command)
{
    Boolean ok;
    ushort reply;

    ok = True;
    switch (command)
        {
        case cmValid:
            ok = isValid;
            if (!ok)
                messageBox(mfError | mfOKButton, "Error opening file (%s).", fileName);
            break;

        case cmQuit:
        case cmClose:

            // Есть открытые формы, которые нельзя закрыть?
            if (message(TProgram::deskTop, evBroadcast, cmCanCloseForm, this) == NULL)
                ok = True;
            else
                ok = False;

            // Есть изменённые данные?
            if (ok && modified)
                {
                select();
                reply = messageBox("Database has been modified. Save? ",
                                    mfYesNoCancel);
                switch (reply)
                    {
                    case cmYes:
                        ok = saveList();
                        break;
                    case cmNo:
                        modified = False;       // Отказываемся от изменений.
                        break;
                    default:
                        ok = False;             // Отменяем запрос закрытия.
                        break;
                    }
                }
            break;
        }
    if (ok)
        return TDialog::valid(command);
    else
        return False;
}

