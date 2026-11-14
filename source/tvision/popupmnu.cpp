/*------------------------------------------------------------*/
/* файл — popupmnu.cpp */
/*                                                            */
/* функции:  */
/*          popupMenu                                         */
/*------------------------------------------------------------*/

#define Uses_TMenuPopup
#define Uses_TMenu
#define Uses_TProgram
#define Uses_TDeskTop
#include <tvision/tv.h>

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  popupMenu                                                             */
/*                                                                        */
/* Создаёт и выполняет TMenuPopup на рабочем столе. */
/*                                                                        */
/* Аргументы: */
/*                                                                        */
/* where — опорная позиция в абсолютных координатах. */
/* Левый верхний угол всплывающего меню располагается */
/* в (where.x, where.y + 1). */
/*                                                                        */
/* aMenu — цепочка пунктов меню. Функция забирает владение */
/* элементами; переданная ссылка становится недействительной */
/* после вызова. */
/*                                                                        */
/* receiver — если не null, событие evCommand с */
/* выбранной командой меню помещается в этот объект */
/* через putEvent. */
/*                                                                        */
/* Возвращает: */
/*                                                                        */
/* Выбранная команда, если она есть. */
/*                                                                        */
/*------------------------------------------------------------------------*/

static void autoPlacePopup(TMenuPopup *, TPoint);

ushort popupMenu(TPoint where, TMenuItem &aMenu, TGroup *receiver)
{
    ushort res = 0;
    TGroup *app = TProgram::application;
    if (app)
    {
        {
            TPoint p = app->makeLocal(where);
            TRect bounds(p, p);
            TMenu *menu = new TMenu(aMenu);
            TMenuPopup *menuPopup = new TMenuPopup(bounds, menu);
            autoPlacePopup(menuPopup, p);
            // Выполняем и освобождаем меню.
            res = app->execView(menuPopup);
            TObject::destroy(menuPopup);
        }
        // Создаём событие.
        if (res && receiver)
        {
            TEvent event = {};
            event.what = evCommand;
            event.message.command = res;
            receiver->putEvent(event);
        }
    }
    return res;
}

static void autoPlacePopup(TMenuPopup *m, TPoint p)
// Предусловие: TMenuPopup создан с bounds = TRect(p, p).
{
    TGroup *app = TProgram::application;
    // Изначально меню находится выше p, поэтому его нужно переместить.
    TRect r = m->getBounds();
    // Для удобства меню не должно выходить за правый нижний угол рабочего
    // стола.
    {
        TPoint d = app->size - p;
        r.move(min(m->size.x, d.x),
               min(m->size.y + 1, d.y));
    }
    // Если после этого меню содержит p, пытаемся расположить его удобнее.
    if (r.contains(p) && r.b.y - r.a.y < p.y)
        r.move(0, -(r.b.y - p.y));
    m->setBounds(r);
}
