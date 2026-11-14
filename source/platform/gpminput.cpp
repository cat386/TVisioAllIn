#ifdef HAVE_GPM

#define Uses_TEvent
#define Uses_TKeys
#include <tvision/tv.h>

#include <internal/gpminput.h>
#include <internal/dispbuff.h>
#include <internal/linuxcon.h>
#include <linux/keyboard.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

namespace tvision
{

// Paths used by the GPM library and server.
static const char gpmCtlPath[] = "/dev/gpmctl";
static const char gpmDevfsConsole[] = "/dev/vc/0";
static const char gpmOldConsole[] = "/dev/tty0";

int GpmInput::open(gpm::Connect &conn) noexcept
{
    // Determine the virtual console number of the current terminal, as
    // 'Gpm_Open' does: take the name of the first standard stream that is a
    // terminal (e.g. "/dev/tty3") and parse the number at the position where
    // the console name has its trailing '0' (e.g. "/dev/tty0").
    struct stat st;
    const char *consoleName = stat(gpmDevfsConsole, &st) == 0 ? gpmDevfsConsole
                            : stat(gpmOldConsole, &st) == 0 ? gpmOldConsole
                            : nullptr;
    if (!consoleName)
        return -1;
    const char *tty = nullptr;
    for (int fd : {0, 1, 2})
        if (isatty(fd) && (tty = ttyname(fd)) != nullptr)
            break;
    size_t prefixLength = strlen(consoleName) - 1;
    if (!tty || strlen(tty) < prefixLength)
        return -1;
    conn.pid = getpid();
    conn.vc = atoi(&tty[prefixLength]);
    {
        // 'Gpm_Open' fails if it cannot open the terminal.
        int consoleFd = ::open(tty, O_WRONLY | O_CLOEXEC);
        if (consoleFd < 0)
            return -1;
        close(consoleFd);
    }

    // Connect to the control socket.
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, gpmCtlPath, sizeof(gpmCtlPath));
    socklen_t addrLen = sizeof(addr.sun_family) + strlen(gpmCtlPath);
    if (connect(fd, (struct sockaddr *) &addr, addrLen) < 0)
    {
        // Like 'Gpm_Open', try opening '/dev/gpmctl' as a character device.
        close(fd);
        if ((fd = ::open(gpmCtlPath, O_RDWR)) == -1)
            return -1;
        if (fstat(fd, &st) == -1 || (st.st_mode & S_IFMT) != S_IFCHR)
        {
            close(fd);
            return -1;
        }
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);

    // Send the connection request.
    ssize_t r;
    while ((r = write(fd, &conn, sizeof(conn))) == -1 && errno == EINTR);
    if (r != (ssize_t) sizeof(conn))
    {
        close(fd);
        return -1;
    }
    return fd;
}

GpmInput *GpmInput::create(DisplayBuffer &displayBuf) noexcept
{
    gpm::Connect conn {};
    conn.eventMask = gpm::DOWN | gpm::UP | gpm::DRAG | gpm::MOVE;
    conn.defaultMask = 0; // Disable cursor drawing by the server.
    // Report keyboard modifiers except Shift, which terminal emulators
    // usually reserve for selecting text. Here GPM will do the same.
    conn.minMod = 0;
    conn.maxMod = (ushort) ~(1 << KG_SHIFT);

    int fd = open(conn);
    if (fd != -1)
        return new GpmInput(fd, displayBuf);
    return nullptr;
}

inline GpmInput::GpmInput(int fd, DisplayBuffer &aDisplayBuf) noexcept :
    InputAdapter(fd),
    displayBuf(aDisplayBuf)
{
}

GpmInput::~GpmInput()
{
    displayBuf.setCursorVisibility(false);
    close(handle);
}

bool GpmInput::readEvent(gpm::Event &gpmEvent) noexcept
{
    ssize_t r;
    while ((r = read(handle, &gpmEvent, sizeof(gpmEvent))) == -1 && errno == EINTR);
    if (r != (ssize_t) sizeof(gpmEvent))
        return false;
    // Let coordinates begin at zero instead of one.
    --gpmEvent.x;
    --gpmEvent.y;
    return true;
}

void GpmInput::fitEvent(gpm::Event &gpmEvent) noexcept
{
    short &x = gpmEvent.x, &y = gpmEvent.y;
    x = (short) min(max(x, 0), displayBuf.size.x - 1);
    y = (short) min(max(y, 0), displayBuf.size.y - 1);
}

static constexpr struct { uchar gpm, mb; } gpmButtonFlags[] =
{
    {gpm::B_LEFT, mbLeftButton},
    {gpm::B_RIGHT, mbRightButton},
    {gpm::B_MIDDLE, mbMiddleButton},
};

bool GpmInput::getEvent(TEvent &ev) noexcept
{
    gpm::Event gpmEvent;
    if (readEvent(gpmEvent))
    {
        fitEvent(gpmEvent);
        displayBuf.setCursorPosition(gpmEvent.x, gpmEvent.y);
        // Do not show the cursor until we receive at least one mouse event.
        displayBuf.setCursorVisibility(true);

        ev.what = evMouse;
        ev.mouse.where.x = gpmEvent.x;
        ev.mouse.where.y = gpmEvent.y;

        for (const auto &flag : gpmButtonFlags)
            if (gpmEvent.buttons & flag.gpm)
            {
                if (gpmEvent.type & gpm::DOWN)
                    buttonState |= flag.mb;
                if (gpmEvent.type & gpm::UP)
                    buttonState &= ~flag.mb;
            }
        ev.mouse.buttons = buttonState;

        if (gpmEvent.wdy)
            ev.mouse.wheel = gpmEvent.wdy > 0 ? mwUp : mwDown;
        else if (gpmEvent.wdx)
            ev.mouse.wheel = gpmEvent.wdx > 0 ? mwRight : mwLeft;
        else
            ev.mouse.wheel = 0;

        ev.mouse.controlKeyState =
            LinuxConsoleInput::convertLinuxKeyModifiers(gpmEvent.modifiers);

        return true;
    }
    return false;
}

} // namespace tvision

#endif // HAVE_GPM
