#include <internal/termdisp.h>

#ifdef _TV_UNIX

#include <internal/terminfo.h>
#include <internal/conctl.h>
#include <internal/getenv.h>
#include <internal/strings.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

namespace tvision
{

static std::string stripPadding(TStringView s) noexcept;

inline TerminalDisplay::TerminalDisplay(ConsoleCtl &aCon, const TermInfo &aTermInfo) noexcept :
    termInfo(aTermInfo),
    con(aCon),
    ansiScreenWriter(aCon, TermCap::getDisplayCapabilities(aCon, *this))
{
    // Ncurses performs terminal operations on the output file descriptor;
    // fall back to the input one if it is not a terminal.
    for (int fd : {con.out(), con.in()})
        if (tcgetattr(fd, &shellMode) == 0)
        {
            ttyFd = fd;
            shellModeSaved = true;
            break;
        }

    if (shellModeSaved)
    {
        // Same settings as Ncurses' 'newterm()': no echo, no CR/NL
        // translation on input or output.
        struct termios mode = shellMode;
        mode.c_lflag &= ~(ECHO | ECHONL);
        mode.c_iflag &= ~(ICRNL | INLCR | IGNCR);
        mode.c_oflag &= ~(ONLCR);
        applyTermios(mode);
    }

    // Enter the alternate screen and leave the terminal in a known state,
    // with the same sequence as Ncurses' 'newterm()' and first 'refresh()':
    // enable the alternate character set, enter the alternate screen, reset
    // the scroll region, reset attributes, leave insert mode, enable automatic
    // margins, reset colors and clear the screen.
    putCap("enacs");
    putCap("smcup");
    size = con.getSize();
    if (const char *csr = termInfo.getStr("csr"))
        if (size.y > 0)
        {
            // Reset the scroll region to the whole screen.
            int params[] = {0, size.y - 1};
            std::string seq = TermInfo::tparm(csr, params, 2);
            putString({seq.data(), seq.size()});
        }
    resetAttributes();
    putCap("rmir");
    putCap("smam");
    if (termInfo.getColorCount() > 0)
    {
        // Like Ncurses' 'start_color()'.
        putCap("op");
        putCap("op");
        resetAttributes();
    }
    clearScreenInitially();
}

void TerminalDisplay::moveCursorToLowerLeft() noexcept
// Like Ncurses' 'endwin()', move the cursor to the lower left corner using
// the cheapest of: direct cursor addressing ('cup'), the 'lower left'
// capability ('ll') or 'home' followed by moving down.
{
    TPoint curSize = con.getSize();
    if (curSize.y <= 0)
        return;
    int lastRow = curSize.y - 1;
    std::string best;
    bool found = false;
    // The cost of a sequence is its length, without padding.
    auto consider = [&] (std::string &&seq) {
        seq = stripPadding({seq.data(), seq.size()});
        if (!found || seq.size() < best.size())
        {
            best = std::move(seq);
            found = true;
        }
    };
    if (const char *cup = termInfo.getStr("cup"))
    {
        int params[] = {lastRow, 0};
        consider(TermInfo::tparm(cup, params, 2));
    }
    if (const char *home = termInfo.getStr("home"))
    {
        std::string seq = home;
        if (lastRow > 0)
        {
            const char *cud = termInfo.getStr("cud");
            const char *cud1 = termInfo.getStr("cud1");
            if (cud)
            {
                int params[] = {lastRow};
                seq += TermInfo::tparm(cud, params, 1);
            }
            else if (cud1 && strcmp(cud1, "\n") != 0)
                for (int i = 0; i < lastRow; ++i)
                    seq += cud1;
            else
                seq.clear();
        }
        if (!seq.empty())
            consider(std::move(seq));
    }
    if (const char *ll = termInfo.getStr("ll"))
        consider(ll);
    if (found)
        putString({best.data(), best.size()});
}

void TerminalDisplay::resetAttributes() noexcept
{
    // Ncurses resorts to leaving the alternate character set mode when the
    // terminal cannot reset attributes.
    if (!putCap("sgr0"))
        putCap("rmacs");
}

void TerminalDisplay::clearScreenInitially() noexcept
{
    // Same strategy as Ncurses: use the 'clear' capability if available.
    // Otherwise, clear to the end of the screen from the home position or,
    // as a last resort, overwrite the screen with spaces.
    if (putCap("clear"))
        return;
    if (termInfo.getStr("home") && termInfo.getStr("ed"))
    {
        putCap("home");
        putCap("ed");
        return;
    }
    if (size.x > 0 && size.y > 0)
    {
        char spaces[256];
        memset(spaces, ' ', sizeof(spaces));
        size_t count = (size_t) size.x * size.y;
        while (count > 0)
        {
            size_t n = min(count, sizeof(spaces));
            con.write(spaces, n);
            count -= n;
        }
    }
}

TerminalDisplay &TerminalDisplay::create(ConsoleCtl &con) noexcept
{
    const TermInfo &termInfo = TermInfo::get(getenv("TERM"));
    return *new TerminalDisplay(con, termInfo);
}

TerminalDisplay::~TerminalDisplay()
{
    // Same sequence as Ncurses' 'endwin()': restore the cursor, move it to
    // the bottom left corner, leave the alternate screen and the keypad
    // transmit mode and restore the terminal settings.
    if (cursorState != -1 && cursorState != 1)
        putCap("cnorm");
    moveCursorToLowerLeft();
    putCap("rmcup");
    con.write("\r", 1);
    setKeypadMode(false);
    if (shellModeSaved)
        applyTermios(shellMode);
}

bool TerminalDisplay::applyTermios(const struct termios &mode) noexcept
{
    if (ttyFd < 0)
        return false;
    int res;
    while ((res = tcsetattr(ttyFd, TCSADRAIN, &mode)) == -1 && errno == EINTR);
    return res == 0;
}

void TerminalDisplay::setRawInputMode() noexcept
{
    struct termios mode;
    if (ttyFd >= 0 && tcgetattr(ttyFd, &mode) == 0)
    {
        // raw(): no line buffering, no signals, no flow control.
        mode.c_lflag &= ~(ICANON | ISIG | IEXTEN);
        mode.c_iflag &= ~(IXON | BRKINT | PARMRK);
        mode.c_cc[VMIN] = 1;
        mode.c_cc[VTIME] = 0;
        // noecho().
        mode.c_lflag &= ~(ECHO | ECHONL);
        // nonl().
        mode.c_iflag &= ~(ICRNL);
        // meta(TRUE): receive 8-bit characters.
        mode.c_iflag &= ~(ISTRIP);
        mode.c_cflag = (mode.c_cflag & ~CSIZE) | CS8;
        applyTermios(mode);
    }
}

void TerminalDisplay::setKeypadMode(bool enable) noexcept
{
    if (enable != keypadEnabled)
    {
        putCap(enable ? "smkx" : "rmkx");
        keypadEnabled = enable;
    }
}

static bool isPadding(TStringView s, size_t &length) noexcept
// Pre: 's' begins with "$<".
// Padding specifications look like "$<5>", "$<2.5*>" or "$<10/>".
{
    size_t i = 2;
    bool digits = false;
    while (i < s.size() && (('0' <= s[i] && s[i] <= '9') || s[i] == '.'))
        digits = true, ++i;
    while (i < s.size() && (s[i] == '*' || s[i] == '/'))
        ++i;
    if (digits && i < s.size() && s[i] == '>')
    {
        length = i + 1;
        return true;
    }
    return false;
}

static std::string stripPadding(TStringView s) noexcept
{
    std::string result;
    size_t i = 0, padLength;
    while (i < s.size())
    {
        if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '<' && isPadding(s.substr(i), padLength))
            i += padLength;
        else
            result += s[i++];
    }
    return result;
}

void TerminalDisplay::putString(TStringView s) noexcept
// Writes a terminfo string, removing padding specifications (which are
// only meaningful for very old terminals), like Ncurses' 'tputs()'.
{
    size_t begin = 0, i = 0, padLength;
    while (i < s.size())
    {
        if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '<' && isPadding(s.substr(i), padLength))
        {
            if (i > begin)
                con.write(&s[begin], i - begin);
            i += padLength;
            begin = i;
        }
        else
            ++i;
    }
    if (i > begin)
        con.write(&s[begin], i - begin);
}

bool TerminalDisplay::putCap(const char *name) noexcept
{
    if (const char *value = termInfo.getStr(name))
    {
        putString(value);
        return true;
    }
    return false;
}

TPoint TerminalDisplay::reloadScreenInfo() noexcept
{
    size = con.getSize();
    ansiScreenWriter.reset();
    return size;
}

int TerminalDisplay::getColorCount() noexcept
{
    return termInfo.getColorCount();
}

TPoint TerminalDisplay::getFontSize() noexcept
{
    return con.getFontSize();
}

void TerminalDisplay::clearScreen() noexcept
{
    ansiScreenWriter.clearScreen();
}

void TerminalDisplay::writeCell( TPoint pos, TStringView text, TColorAttr attr,
                                 bool doubleWidth ) noexcept
{
    ansiScreenWriter.writeCell(pos, text, attr, doubleWidth);
}

void TerminalDisplay::setCaretPosition(TPoint pos) noexcept
{
    ansiScreenWriter.setCaretPosition(pos);
}

void TerminalDisplay::setCaretSize(int size) noexcept
{
/* The caret is the keyboard cursor. If size is 0, the caret is hidden. The
 * other possible values are from 1 to 100, theoretically, and represent the
 * percentage of the character cell the caret fills.
 * https://docs.microsoft.com/en-us/windows/console/console-cursor-info-str
 *
 * Terminals support only three levels: invisible (0), normal (1) and
 * very visible (2). They don't make a difference in all terminals, but
 * we can try mapping them to the values requested by Turbo Vision. */
    int state = size > 0 ? size == 100 ? 2 : 1 : 0;
    if (state != cursorState)
    {
        const char *cap = state == 0 ? "civis" : state == 1 ? "cnorm" : "cvvis";
        // Like Ncurses' 'curs_set()', do nothing if the terminal does not
        // support the requested visibility.
        if (putCap(cap))
            cursorState = state;
    }
}

void TerminalDisplay::flush() noexcept
{
    ansiScreenWriter.flush();
}

} // namespace tvision

#endif // _TV_UNIX
