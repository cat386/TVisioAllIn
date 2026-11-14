#ifndef TVISION_TERMDISP_H
#define TVISION_TERMDISP_H

#include <internal/platform.h>

#ifdef _TV_UNIX

#include <internal/ansiwrit.h>
#include <termios.h>

namespace tvision
{

class ConsoleCtl;
class TermInfo;

// TerminalDisplay manages the terminal on Unix systems without depending on
// external libraries (it replaces the former Ncurses-based display).
//
// It is responsible for:
// * Saving and restoring the terminal settings (termios).
// * Entering and leaving the alternate screen ('smcup'/'rmcup').
// * Enabling and disabling the keypad transmit mode ('smkx'/'rmkx').
// * Changing the visibility of the cursor ('civis'/'cnorm'/'cvvis').
// * Reporting the number of colors supported by the terminal.
//
// The terminal capabilities are read from the terminfo database by 'TermInfo'.
// Screen contents are printed using ANSI escape sequences by 'AnsiScreenWriter'.

class TerminalDisplay final : public DisplayAdapter
{
public:

    // The lifetime of 'con' exceeds that of the returned object.
    static TerminalDisplay &create(ConsoleCtl &con) noexcept;
    ~TerminalDisplay();

    const TermInfo &termInfo;

    // Puts the terminal in raw mode (no line buffering, no echo, no signal
    // generation, no CR/NL translation), like Ncurses' 'raw()', 'noecho()'
    // and 'nonl()'.
    void setRawInputMode() noexcept;
    // Enables or disables the keypad transmit mode, like Ncurses' 'keypad()'.
    void setKeypadMode(bool enable) noexcept;

private:

    ConsoleCtl &con;
    int ttyFd {-1};
    struct termios shellMode;
    bool shellModeSaved {false};
    // Cursor visibility: -1 (unknown), 0 (invisible), 1 (normal), 2 (very visible).
    int cursorState {-1};
    bool keypadEnabled {false};
    TPoint size {};
    AnsiScreenWriter ansiScreenWriter;

    TerminalDisplay(ConsoleCtl &, const TermInfo &) noexcept;

    bool putCap(const char *name) noexcept;
    void putString(TStringView) noexcept;
    void resetAttributes() noexcept;
    void moveCursorToLowerLeft() noexcept;
    void clearScreenInitially() noexcept;
    bool applyTermios(const struct termios &) noexcept;

    TPoint reloadScreenInfo() noexcept override;

    int getColorCount() noexcept override;
    TPoint getFontSize() noexcept override;

    void writeCell(TPoint, TStringView, TColorAttr, bool) noexcept override;
    void setCaretPosition(TPoint) noexcept override;
    void setCaretSize(int) noexcept override;
    void clearScreen() noexcept override;
    void flush() noexcept override;
};

} // namespace tvision

#endif // _TV_UNIX

#endif // TVISION_TERMDISP_H
