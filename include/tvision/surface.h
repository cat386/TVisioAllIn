/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   SURFACE.H                                                             */
/*                                                                         */
/* Определяет классы TDrawSurface и TSurfaceView. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_TDrawSurface ) && !defined( __TDrawSurface )
#define __TDrawSurface

// TDrawSurface содержит двумерный буфер TScreenCell, доступный для
// произвольной записи.

class TDrawSurface
{

    size_t dataLength;
    TScreenCell _FAR *data;

public:

    TPoint size;

    TDrawSurface() noexcept;
    TDrawSurface(TPoint aSize) noexcept;
    ~TDrawSurface();

    void resize(TPoint aSize);
    void grow(TPoint aDelta);
    void clear();

    // Внимание: границы не проверяются.
    TScreenCell _FAR &at(int y, int x);
    const TScreenCell _FAR &at(int y, int x) const;

};

inline void TDrawSurface::grow(TPoint aDelta)
{
    resize(size + aDelta);
}

inline TScreenCell _FAR &TDrawSurface::at(int y, int x)
{
    return data[y*size.x + x];
}

inline const TScreenCell _FAR &TDrawSurface::at(int y, int x) const
{
    return data[y*size.x + x];
}

#endif

/* ---------------------------------------------------------------------- */
/* класс TSurfaceView */
/*                                                                        */
/* Структура палитры */
/* 1 = Пустая область */
/* ---------------------------------------------------------------------- */

#if defined( Uses_TSurfaceView ) && !defined( __TSurfaceView )
#define __TSurfaceView

// TSurfaceView отображает область TDrawSurface от delta до {delta.x +
// size.x, delta.y + size.y}. Области вне границ, либо всё представление
// при нулевом surface, заполняются пробелами.
//

// Цвет пустой области соответствует цвету неактивной рамки TWindow и TDialog.

class TSurfaceView : public TView
{

public:

    const TDrawSurface _FAR *surface;
    TPoint delta;

    TSurfaceView(const TRect &bounds, const TDrawSurface _FAR *aSurface=0) noexcept;

    virtual void draw();
    virtual TPalette& getPalette() const;

};

#endif
