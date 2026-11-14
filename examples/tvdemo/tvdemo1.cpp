/*----------------------------------------------------------*/
/*                                                          */
/* Исходный файл TVDEMO для Turbo Vision. */
/*                                                          */
/*----------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TView
#define Uses_TRect
#define Uses_TStatusLine
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TKeys
#define Uses_MsgBox
#define Uses_fpstream
#define Uses_TEvent
#define Uses_TDeskTop
#define Uses_TApplication
#define Uses_TWindow
#define Uses_TDeskTop
#define Uses_TScreen

#include <tvision/tv.h>

#include "tvdemo.h"
#include "tvcmds.h"
#include "gadgets.h"
#include "fileview.h"
#include "puzzle.h"
#include "demohelp.h"
#include "evntview.h"
#include <tvision/help.h>

#include <stdio.h>
#include <string.h>

#ifdef __FLAT__
#define HELP_FILENAME "demohelp.h32"
#else
#define HELP_FILENAME "demohelp.h16"
#endif

// main создаёт объект приложения. Конструктор выполняет инициализацию.
// Вызов run() из TProgram запускает цикл обработки событий, а деструктор
// освобождает все объекты. Имена файлов можно передать в командной
// строке для автоматического открытия.
//
//
//
//

int main(int argc, char **argv)
{
    TVDemo *demoProgram = new TVDemo(argc, argv);

    demoProgram->run();

    TObject::destroy( demoProgram );
    return 0;
}


// Конструктор приложения. Параметры командной строки трактуются как
// имена файлов и открываются. Разрешены маски: для них открывается
// диалог с соответствующим путём поиска.
//
//

TVDemo::TVDemo( int argc, char **argv ) :
    TProgInit( &TVDemo::initStatusLine,
               &TVDemo::initMenuBar,
               &TVDemo::initDeskTop )
{
    TView *w;
    char fileSpec[128];
    int len;

    TRect r = getExtent();                      // Создаём представление часов.
    r.a.x = r.b.x - 9;      r.b.y = r.a.y + 1;
    clock = new TClockView( r );
    clock->growMode = gfGrowLoX | gfGrowHiX;
    insert(clock);

    r = getExtent();                            // Создаём представление памяти.
    r.a.x = r.b.x - 13;     r.a.y = r.b.y - 1;
    heap = new THeapView( r );
    heap->growMode = gfGrowAll;
    insert(heap);

    while (--argc > 0)                              // Показываем файлы, указанные
        {                                           // в командной строке.
        strcpy( fileSpec, *++argv );
        len = strlen( fileSpec );
        if( fileSpec[len-1] == '\\' || fileSpec[len-1] == '/' )
            strcat( fileSpec, "*.*" );
        if( strchr( fileSpec, '*' ) || strchr( fileSpec, '?' ) )
            openFile( fileSpec );
        else
            {
            w = validView( new TFileWindow( fileSpec ) );
            if( w != 0 )
                deskTop->insert(w);
            }
        }

}


// DemoApp::getEvent() — цикл проверки запросов контекстной справки.
//
//
//

void TVDemo::getEvent(TEvent &event)
{
    TWindow *w;
    THelpFile *hFile;
    fpstream *helpStrm;
    static Boolean helpInUse = False;

    TApplication::getEvent(event);
    printEvent(event);
    switch (event.what)
        {
        case evCommand:
            if ((event.message.command == cmHelp) && ( helpInUse == False)) 
                {
                helpInUse = True;
                helpStrm = new fpstream(HELP_FILENAME, ios::in|ios::binary);
                hFile = new THelpFile(*helpStrm);
                if (!helpStrm)
                    {
                    messageBox("Could not open help file", mfError | mfOKButton);
                    delete hFile;
                    }
                else
                    {
                    w = new THelpWindow(hFile, getHelpCtx());
                    if (validView(w) != 0)
                        {
                        execView(w);
                        destroy( w );
                        }
                    clearEvent(event);
                    }
                helpInUse = False;
                }
            else if (event.message.command == cmVideoMode)
                {
                int newMode = TScreen::screenMode ^ TDisplay::smFont8x8;
                setScreenMode((ushort)newMode);
                }
            break;
        case evMouseDown:
            if (event.mouse.buttons == mbRightButton)
                event.what = evNothing;
            break;
        }

}  

// Создаём строку состояния.
//
//

TStatusLine *TVDemo::initStatusLine( TRect r )
{
    r.a.y = r.b.y - 1;

    return (new TStatusLine( r,
      *new TStatusDef( 0, 50 ) +
        *new TStatusItem( "~F1~ Help", kbF1, cmHelp ) +
        *new TStatusItem( "~Alt-X~ Exit", kbAltX, cmQuit ) +
        *new TStatusItem( 0, kbShiftDel, cmCut ) +
        *new TStatusItem( 0, kbCtrlIns, cmCopy ) +
        *new TStatusItem( 0, kbShiftIns, cmPaste ) +
        *new TStatusItem( 0, kbAltF3, cmClose ) +
        *new TStatusItem( 0, kbF10, cmMenu ) +
        *new TStatusItem( 0, kbF5, cmZoom ) +
        *new TStatusItem( 0, kbCtrlF5, cmResize ) +
      *new TStatusDef( 50, 0xffff ) +
        *new TStatusItem( "Howdy", kbF1, cmHelp )
        )
    );
}


// Функция головоломки.
//
//

void TVDemo::puzzle()
{
    TPuzzleWindow *puzz = (TPuzzleWindow *) validView(new TPuzzleWindow);

    if(puzz != 0)
        {
        puzz->helpCtx = hcPuzzle;
        deskTop->insert(puzz);
	}
}


// retrieveDesktop() восстанавливает ранее сохранённый рабочий стол.
//
//

void TVDemo::retrieveDesktop()
{
    if (!ifstream("TVDEMO.DST").good())
        messageBox("Could not find desktop file", mfOKButton | mfError);
    else 
        {
        fpstream *f = new fpstream("TVDEMO.DST", ios::in|ios::binary);
        if( !f )
            messageBox("Could not open desktop file", mfOKButton | mfError);
        else
           {
           TVDemo::loadDesktop(*f);
           if( !f )
               messageBox("Error reading desktop file", mfOKButton | mfError);
           }
        delete f;
        }
}

// saveDesktop() сохраняет рабочий стол, вызывая storeDesktop.
//
//

void TVDemo::saveDesktop()
{
    fpstream *f = new fpstream("TVDEMO.DST", ios::out|ios::binary);

    if( f )
        {
        TVDemo::storeDesktop(*f);
        if( !f )
            {
            messageBox("Could not create TVDEMO.DST.", mfOKButton | mfError);
            delete f;
            ::remove("TVDEMO.DST");
            return;
            }
        }
    delete f;
}

// writeView() записывает объект представления в файл ресурсов.
//
//

static void writeView(TView *p, void *strm)
{
   fpstream *s = (fpstream *) strm;
   if (p != TProgram::deskTop->last)
      *s << p;
}

// storeDesktop() сохраняет рабочий стол в файле ресурсов.
//
//

void TVDemo::storeDesktop(fpstream& s)
{
  deskTop->forEach(::writeView, &s);
  s << 0;
}
