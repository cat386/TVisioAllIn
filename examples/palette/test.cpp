// TEST.CPP — основной тестовый модуль примера Turbo Vision.
//
//

#define Uses_TKeys
#define Uses_TRect
#define Uses_TEvent
#define Uses_TMenuBar
#define Uses_TMenu
#define Uses_TMenuItem
#define Uses_TDialog
#define Uses_TButton
#define Uses_TStaticText
#define Uses_TDeskTop
#define Uses_TApplication
#define Uses_MsgBox
#include <tv.h>
#include "cmds.h"       // Пользовательский набор команд приложения.
                        //
#include "test.h"       // Определение класса приложения.
#include "palette.h"    // Определение класса TTestWindow.

// TTestApp — конструктор.
//
//

#define cpTestAppC  "\x3E\x2D\x72\x5F\x68\x4E"
#define cpTestAppBW "\x07\x07\x0F\x70\x78\x7F"
#define cpTestAppM  "\x07\x0F\x70\x09\x0F\x79"

TTestApp::TTestApp() :
    TProgInit( initStatusLine, initMenuBar, initDeskTop )
{
}

// initMenuBar инициализирует строку меню; вызывается конструктором
// виртуальной базы TProgInit.
//
//

TMenuBar *TTestApp::initMenuBar( TRect bounds )
{
    bounds.b.y = bounds.a.y + 1;
    TMenuBar *mainMenu = new TMenuBar (bounds, new TMenu(
        *new TMenuItem("~A~bout...", cmAbout, kbAltA, hcNoContext, 0,
         new TMenuItem("~P~alette", cmPaletteView, kbAltP, hcNoContext, 0,
         new TMenuItem("E~x~it", cmQuit, kbAltX)))
        ));
    return( mainMenu );
}

// handleEvent обрабатывает выбор пунктов меню и строки состояния.
//
//
//

void TTestApp::handleEvent(TEvent& event)
{
    TApplication::handleEvent(event);
    switch(event.what)
    {
    case evCommand:             // Обрабатываем события команд.
        switch(event.message.command)
        {
        case cmAbout:           // Открываем диалоговое окно.
            aboutDlg();
            break;
        case cmPaletteView:     // Открываем пример палитры.
            paletteView();
            break;
        default:                // Эти события не обработаны.
            return;
        }
        break;
    default:                    // Эти события не обработаны.
        return;
    }
    clearEvent(event);          // Сбрасываем обработанные события.
                                //
}

// getPalette определяет новую системную палитру. Системная палитра
// должна поддерживать три типа отображения: цветное, чёрно-белое и
// монохромное. Объединение строк в Borland C++ присоединяет расширение к
// стандартной системной палитре.
//
//
//

TPalette& TTestApp::getPalette() const
{
    static TPalette
        newColor( cpAppColor cpTestAppC,
                  sizeof( cpAppColor cpTestAppC )-1 ),
        newBlackWhite( cpAppBlackWhite cpTestAppBW,
                       sizeof( cpAppBlackWhite cpTestAppBW)-1 ),
        newMonochrome( cpAppMonochrome cpTestAppM,
                       sizeof( cpAppMonochrome cpTestAppM)-1 );
    static TPalette *palettes[] =
        {
        &newColor,
        &newBlackWhite,
        &newMonochrome
        };
    return *(palettes[appPalette]); // Поле 'appPalette' указывает, какая палитра используется: цветная,
                                    // чёрно-белая или монохромная.
                                    //
                                    //
                                    //
}

// aboutDlg создаёт и выполняет диалог «О программе».
//
//
//

void TTestApp::aboutDlg()
{
    TDialog *aboutDlgBox = new TDialog(TRect(0, 0, 47, 13), "About");
    if( validView( aboutDlgBox ) )
    {
        aboutDlgBox->insert(
            new TStaticText(
                TRect(2,1,45,9),
                "\n\003PALETTE EXAMPLE\n \n"
                "\003A Turbo Vision Demo\n \n"
                "\003written by\n \n"
                "\003Borland C++ Tech Support\n"
            ));
        aboutDlgBox->insert(
            new TButton(TRect(18,10,29,12), "OK", cmOK,
                         bfDefault)
            );
        aboutDlgBox->options |= ofCentered;     // По центру экрана.
                                                //
        execView( aboutDlgBox );                // Открываем окно в модальном режиме.
                                                //
        destroy( aboutDlgBox );                 // Уничтожаем окно.
                                                //
    }
}

void TTestApp::paletteView()
{
    TView *view = new TTestWindow;
    if( validView( view ) )
        deskTop->insert( view );
}

int main()
{
    TTestApp testApp;
    testApp.run();
    return 0;
}
