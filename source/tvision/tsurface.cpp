/*------------------------------------------------------------*/
/* файл — tsurface.cpp */
/*                                                            */
/* функции:  */
/* Методы следующих классов. */
/*                      TDrawSurface                          */
/*                      TSurfaceView                          */
/*------------------------------------------------------------*/

#define Uses_TDrawSurface
#define Uses_TSurfaceView
#define Uses_TDrawBuffer
#define Uses_TRect
#include <tvision/tv.h>

#include <stdlib.h>
#include <malloc.h>

#define cpSurfaceView "\x01"

TDrawSurface::TDrawSurface() noexcept :
    dataLength(0),
    data(0)
{
    size.x = size.y = 0;
}

TDrawSurface::TDrawSurface(TPoint aSize) noexcept :
    dataLength(0),
    data(0)
{
    resize(aSize);
}

TDrawSurface::~TDrawSurface()
{
    ::free(data);
}

void TDrawSurface::resize(TPoint aSize)
{
    if (aSize.x > 0 && aSize.y > 0)
    {
        size_t newLength = aSize.x*aSize.y;
        size_t sz = newLength*sizeof(TScreenCell);
        void _FAR *newData;
        if (newLength <= dataLength)
            newData = ::realloc(data, sz);
        else
        {
            ::free(data);
            newData = ::malloc(sz);
        }
        if (newData == 0 && newLength != 0)
            abort();
        data = (TScreenCell _FAR *) newData;
        dataLength = newLength;
#ifndef __BORLANDC__
        // Инициализируем буфер, как TGroup.
        memset(data, 0, sz);
#endif
    }
    else
    {
        ::free(data);
        data = 0;
        dataLength = 0;
    }
    size = aSize;
}

void TDrawSurface::clear()
{
    memset(data, 0, dataLength*sizeof(TScreenCell));
}

TSurfaceView::TSurfaceView( const TRect &bounds,
                            const TDrawSurface _FAR *aSurface ) noexcept :
    TView(bounds),
    surface(aSurface)
{
    delta.x = delta.y = 0;
}

static void fillWithSpaces(TScreenCell *b, int len, TColorAttr c)
{
    TScreenCell cell = {' ', c};
    for (int i = 0; i < len; ++i)
        b[i] = cell;
}

void TSurfaceView::draw()
{
    if (size.x <= 0 || size.y <= 0)
        return;
    TScreenCell *b = (TScreenCell *) alloca(size.x*sizeof(TScreenCell));
    TColorAttr cEmpty = mapColor(1);
    int y;
    if (surface)
    {
        const TRect extent = TRect(TPoint(), size);
        // Прямоугольная область текущего представления для вывода поверхности.
        //
        const TRect clip = TRect(TPoint(), surface->size)
            .move(-delta.x, -delta.y)
            .intersect(extent);
        if ( 0 <= clip.a.x && clip.a.x < clip.b.x &&
             0 <= clip.a.y && clip.a.y < clip.b.y )
        {
            const TScreenCell *data = &surface->at(max(delta.y, 0), max(delta.x, 0));
            if (clip == extent)
                // Поверхность занимает всё представление; доступно прямое копирование.
                for (y = 0; y < size.y; ++y, data += surface->size.x)
                    writeBuf(0, y, size.x, 1, data);
            else
            {
                fillWithSpaces(b, size.x, cEmpty);
                // Заполняем пустую область сверху и снизу.
                writeLine(0,        0, size.x,          clip.a.y, b);
                writeLine(0, clip.b.y, size.x, size.y - clip.b.y, b);
                // Выводим содержимое поверхности.
                if (clip.a.x == 0 && clip.b.x == size.x)
                    // Прямое копирование тоже возможно.
                    for (y = clip.a.y; y < clip.b.y; ++y, data += surface->size.x)
                        writeBuf(0, y, size.x, 1, data);
                else
                    for (y = clip.a.y; y < clip.b.y; ++y, data += surface->size.x)
                    {
                        memcpy(&b[clip.a.x], data, (clip.b.x - clip.a.x)*sizeof(TScreenCell));
                        writeBuf(0, y, size.x, 1, b);
                    }
            }
        }
    }
    else
    {
        fillWithSpaces(b, size.x, cEmpty);
        writeLine(0, 0, size.x, size.y, b);
    }
}

TPalette &TSurfaceView::getPalette() const
{
    static TPalette palette(cpSurfaceView, sizeof(cpSurfaceView) - 1);
    return palette;
}
