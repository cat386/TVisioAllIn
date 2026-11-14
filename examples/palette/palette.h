//
// PALETTE.H - Example view with own palette.
//
//  Copyright (C) Borland International, 1991.
//

#if !defined( _PALETTE_H )
#define _PALETTE_H

// Класс TTestView — представление, выводящее текст случайным цветом.
// Методы: TTestView — конструктор; ~TTestView — деструктор; draw — вывод
// текста; getPalette — получение цвета текста.
//
//
//
//
//
//
//
//

class TTestView : public TView
{
public:
    TTestView( TRect& r );
    virtual ~TTestView() {}
    virtual void draw();
    virtual TPalette& getPalette() const;
private:
};

// TTestWindow содержит TTestView и позволяет перемещать представление по
// экрану и удалять его с рабочего стола почти без дополнительного кода:
// TWindow автоматически вызывает cmClose. Методы: TTestWindow —
// конструктор; getPalette — палитра; sizeLimits — разрешение меньшего
// размера, чем стандартный минимум TWindow.
//
//
//
//
//
//
//

#define TEST_WIDTH   42
#define TEST_HEIGHT  11

class TTestWindow : public TWindow
{
public:
    TTestWindow();
    virtual ~TTestWindow() {}
    virtual TPalette& getPalette() const;
    virtual void sizeLimits( TPoint& min, TPoint& max )
    {
          min.x = max.x = TEST_WIDTH;
          min.y = max.y = TEST_HEIGHT;
    }
};

#endif  // _PALETTE_H
