/*---------------------------------------------------------*/
/*                                                         */
/* TVDemo.h — заголовочный файл для TVDemo.cpp */
/*                                                         */
/*---------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined( __TVDEMO_H )
#define __TVDEMO_H

class TStatusLine;
class TMenuBar;
struct TEvent;
class TPalette;
class THeapView;
class TClockView;
class fpstream;

class TVDemo : public TApplication 
{

public:

    TVDemo( int argc, char **argv );
    static TStatusLine *initStatusLine( TRect r );
    static TMenuBar *initMenuBar( TRect r );
    virtual void handleEvent(TEvent& Event);
    virtual void getEvent(TEvent& event);
//    virtual TPalette& getPalette() const;
    virtual void idle();              // Обновляет представления памяти и часов.

private:

    THeapView *heap;                  // Представление памяти.
    TClockView *clock;                // Представление часов.

    void aboutDlgBox();               // Окно «О программе».
    void puzzle();                    // Головоломка.
    void calendar();                  // Календарь.
    void asciiTable();                // Таблица ASCII.
    void calculator();                // Калькулятор.
    void eventViewer();
    void printEvent(const TEvent &);
    void chBackground();              // Узор фона.
    void openFile( const char *fileSpec );  // Просмотр файлов.
    void changeDir();                 // Смена каталога.
    void mouse();                     // Диалог настройки мыши.
    void colors();                    // Диалог настройки цветов.
    void outOfMemory();               // Для функции validView().
    void loadDesktop(fpstream& s);    // Загружаем и восстанавливаем
    void retrieveDesktop();           // ранее сохранённый рабочий стол.
    void storeDesktop(fpstream& s);   // Сохраняем текущий рабочий стол
    void saveDesktop();               // в файле ресурсов.

};

#endif // __TVDEMO_H
