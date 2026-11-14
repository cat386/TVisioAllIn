#ifndef TVISION_GPMINPUT_H
#define TVISION_GPMINPUT_H

#include <internal/platform.h>

#ifdef HAVE_GPM

#include <internal/recordio.h>

#define Uses_TPoint
#define Uses_TEvent
#include <tvision/tv.h>

namespace tvision
{

class DisplayBuffer;

// Клиент сервера GPM (General Purpose Mouse), обеспечивающего поддержку
// мыши в консоли Linux. Реализация клиентской части протокола автономна
// и не требует связывания с libgpm. Протокол прост: клиент подключается
// к Unix-сокету /dev/gpmctl, отправляет структуру Gpm_Connect и затем
// получает структуры Gpm_Event.
//
//

namespace gpm
{

// Эти определения двоично совместимы с определениями из <gpm.h>.

struct Connect
{
    unsigned short eventMask, defaultMask;
    unsigned short minMod, maxMod;
    int pid;
    int vc;
};

struct Event
{
    unsigned char buttons, modifiers;
    unsigned short vc;
    short dx, dy, x, y;
    int type; // Перечисление Gpm_Etype.
    int clicks;
    int margin; // Перечисление Gpm_Margin.
    short wdx, wdy;
};

static_assert(sizeof(Connect) == 16, "");
static_assert(sizeof(Event) == 28, "");

enum : unsigned char
{
    B_DOWN = 32,
    B_UP = 16,
    B_FOURTH = 8,
    B_LEFT = 4,
    B_MIDDLE = 2,
    B_RIGHT = 1,
};

enum : int
{
    MOVE = 1,
    DRAG = 2,
    DOWN = 4,
    UP = 8,
};

} // пространство имён gpm

class GpmInput final : public InputAdapter
{
    uchar buttonState {0};
    RecordReader<sizeof(gpm::Event)> eventReader;
    DisplayBuffer &displayBuf;

    GpmInput(int fd, DisplayBuffer &) noexcept;
    void fitEvent(gpm::Event &) noexcept;

    // Открывает соединение с сервером GPM и возвращает файловый дескриптор
    // либо -1 при ошибке. Эквивалент Gpm_Open.
    static int open(gpm::Connect &) noexcept;
    // Читает событие. Эквивалент Gpm_GetEvent с gpm_zerobased = 1.
    bool readEvent(gpm::Event &) noexcept;

public:

    // Предусловие: displayBuf должен существовать дольше возвращаемого объекта.
    static GpmInput *create(DisplayBuffer &displayBuf) noexcept;
    ~GpmInput();

    bool getEvent(TEvent &ev) noexcept override;
};

} // пространство имён tvision

#else

namespace tvision
{

class GpmInput : public InputAdapter
{
public:
    static GpmInput *create(DisplayBuffer &) noexcept { return nullptr; }
};

} // пространство имён tvision

#endif // HAVE_GPM

#endif // TVISION_GPMINPUT_H
