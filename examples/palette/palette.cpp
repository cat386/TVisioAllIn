// PALETTE.CPP — пример использования системы палитр.
//
//

#define Uses_TView
#define Uses_TWindow
#define Uses_TPalette
#define Uses_TDrawBuffer
#include <tv.h>
#include "palette.h"                    // Определения классов этого модуля
                                        //
#include <stdio.h>                      // Для sprintf().

// Конструктор TTestView
//
//

#define cpTestView "\x9\xA\xB\xC\xD\xE" // В этом представлении доступны шесть цветов.
                                        //

TTestView::TTestView( TRect& r ) : TView( r )
{
}

void TTestView::draw()
{
    TDrawBuffer buf;
    TColorAttr textAttr;
    char text[128];
    for(int i = 1; i <= 6; i++)         // Перебираем палитру из шести элементов.
                                        //
    {
        textAttr = getColor( i );       // Получаем атрибут по заданному индексу.
                                        //
        sprintf(text, " This line uses index %02X, color is %02X ", i, (uchar) textAttr);
        buf.moveStr(0, text, textAttr);      // Записываем в буфер.
        writeLine(0, i-1, size.x, 1, buf);   // Выводим буфер в представление.
                                             //
    }
// Последняя строка этого представления не использует палитры: она всегда
// выводится пурпурным на чёрном фоне.
//
//
    buf.moveStr(0, "   This line bypasses the palettes!    ", 5);
    writeLine(0, 6, size.x, 7, buf);
}

// getPalette создаёт и возвращает палитру с заданными значениями.
//
//

TPalette& TTestView::getPalette() const
{
    static TPalette palette( cpTestView, sizeof(cpTestView)-1 );
    return palette;
}

//
// TTestWindow
//

#define cpTestWindow "\x88\x89\x8A\x8B\x8C\x8D"
// Ещё шесть цветов!
#define cpBlueWindow "\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
// Исходные палитры.
#define cpCyanWindow "\x10\x11\x12\x13\x14\x15\x16\x17"
#define cpGrayWindow "\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"

TTestWindow::TTestWindow() :
    TWindowInit( initFrame ),
    TWindow( TRect(0, 0, TEST_WIDTH, TEST_HEIGHT), 0, wnNoNumber )
{
    TRect r = getExtent();
    r.grow(-2, -2);
    insert( new TTestView(r) );
    options |= ofCentered;
    flags = wfMove | wfClose;
}

// getPalette: как и системная палитра, палитры окон имеют несколько
// вариантов: синий, бирюзовый и серый. Новый набор цветов только один,
// поэтому присоединяем его к каждому варианту с помощью объединения
// соседних строковых литералов компилятором. Поэтому используются
// #define, а не 'const char *'.
//
//
//

TPalette& TTestWindow::getPalette() const
{
    static TPalette blue( cpBlueWindow cpTestWindow,
                          sizeof( cpBlueWindow cpTestWindow )-1
                        );
    static TPalette cyan( cpCyanWindow cpTestWindow,
                          sizeof( cpCyanWindow cpTestWindow )-1
                        );
    static TPalette gray( cpGrayWindow cpTestWindow,
                          sizeof( cpGrayWindow cpTestWindow )-1
                        );
    static TPalette *palettes[] = { &blue, &cyan, &gray };
    return *(palettes[palette]);    // Поле 'palette' задаёт используемую палитру.
                                    //
                                    //
                                    //
                                    //
}
