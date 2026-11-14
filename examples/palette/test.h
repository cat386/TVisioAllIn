// TEST.H — определение класса TTestApp. Класс TTestApp — объект
// приложения, производный от абстрактного TApplication. Методы: TTestApp
// — конструктор; initMenuBar — собственное меню; handleEvent — обработка
// событий меню; aboutDlg — создание и показ окна «О программе».
//
//
//
//
//
//
//
//
//
//

#if !defined( _TEST_H )
#define _TEST_H

class TTestApp : public TApplication
{
public:
    TTestApp();
    static TMenuBar *initMenuBar( TRect r );
    virtual void handleEvent( TEvent& event);
    virtual TPalette& getPalette() const;
private:
    void aboutDlg();
    void paletteView();
};

#endif  // _TEST_H
