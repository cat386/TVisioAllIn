#ifndef TVISION_GPMINPUT_H
#define TVISION_GPMINPUT_H

#include <internal/platform.h>

#ifdef HAVE_GPM

#define Uses_TPoint
#define Uses_TEvent
#include <tvision/tv.h>

namespace tvision
{

class DisplayBuffer;

// Client of the GPM (General Purpose Mouse) server, which provides mouse
// support in the Linux console.
//
// This is a self-contained implementation of the client side of the GPM
// protocol, so that linking to 'libgpm' is not required. The protocol is
// simple: the client connects to the Unix socket '/dev/gpmctl', sends a
// 'Gpm_Connect' structure and then receives 'Gpm_Event' structures.

namespace gpm
{

// These definitions are binary-compatible with those in <gpm.h>.

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
    int type; // enum Gpm_Etype.
    int clicks;
    int margin; // enum Gpm_Margin.
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

} // namespace gpm

class GpmInput final : public InputAdapter
{
    uchar buttonState {0};
    DisplayBuffer &displayBuf;

    GpmInput(int fd, DisplayBuffer &) noexcept;
    void fitEvent(gpm::Event &) noexcept;

    // Opens a connection to the GPM server and returns its file descriptor,
    // or -1 on failure. Equivalent to 'Gpm_Open'.
    static int open(gpm::Connect &) noexcept;
    // Reads an event. Equivalent to 'Gpm_GetEvent' with 'gpm_zerobased = 1'.
    bool readEvent(gpm::Event &) noexcept;

public:

    // Pre: The lifetime of 'displayBuf' must exceed that of the returned object.
    static GpmInput *create(DisplayBuffer &displayBuf) noexcept;
    ~GpmInput();

    bool getEvent(TEvent &ev) noexcept override;
};

} // namespace tvision

#else

namespace tvision
{

class GpmInput : public InputAdapter
{
public:
    static GpmInput *create(DisplayBuffer &) noexcept { return nullptr; }
};

} // namespace tvision

#endif // HAVE_GPM

#endif // TVISION_GPMINPUT_H
