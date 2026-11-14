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

// TerminalDisplay управляет терминалом в Unix без внешних библиотек и
// заменяет прежнюю реализацию на Ncurses. Сохраняет и восстанавливает
// termios; включает и выключает альтернативный экран через smcup/rmcup и
// передачу кодов клавиатуры через smkx/rmkx; меняет видимость курсора
// через civis/cnorm/cvvis; сообщает число поддерживаемых цветов.
// Возможности терминала читает из terminfo класс TermInfo. Содержимое
// экрана выводится последовательностями ANSI через AnsiScreenWriter.
//
//
//
//
//

class TerminalDisplay final : public DisplayAdapter
{
public:

    // con существует дольше возвращаемого объекта.
    static TerminalDisplay &create(ConsoleCtl &con) noexcept;
    ~TerminalDisplay();

    const TermInfo &termInfo;

    // Переводит терминал в необработанный режим: без построчной буферизации,
    // эха, генерации сигналов и преобразования CR/NL. Аналог raw(), noecho()
    // и nonl() в Ncurses.
    void setRawInputMode() noexcept;
    // Включает или отключает передачу кодов клавиатуры, как keypad() в Ncurses.
    void setKeypadMode(bool enable) noexcept;

private:

    ConsoleCtl &con;
    int ttyFd {-1};
    struct termios shellMode;
    bool shellModeSaved {false};
    // Видимость курсора: -1 — неизвестна; 0 — скрыт; 1 — обычный; 2 — хорошо заметный.
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

} // пространство имён tvision

#endif // _TV_UNIX

#endif // TVISION_TERMDISP_H
