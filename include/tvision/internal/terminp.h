#ifndef TVISION_TERMINP_H
#define TVISION_TERMINP_H

#include <internal/platform.h>

#ifdef _TV_UNIX

#define Uses_TKeys
#define Uses_TPoint
#define Uses_TEvent
#include <tvision/tv.h>

#include <internal/termio.h>
#include <vector>

namespace tvision
{

class TerminalDisplay;
class TermInfo;

// Коды клавиш совпадают со значениями Ncurses. TerminalInputGetter::get
// возвращает их для последовательностей, описанных в базе terminfo.
//

namespace termkeys
{

enum : int
{
    KEY_MIN         = 0401,
    KEY_DOWN        = 0402,
    KEY_UP          = 0403,
    KEY_LEFT        = 0404,
    KEY_RIGHT       = 0405,
    KEY_HOME        = 0406,
    KEY_BACKSPACE   = 0407,
    KEY_F0          = 0410,
    KEY_DC          = 0512,
    KEY_IC          = 0513,
    KEY_SF          = 0520,
    KEY_SR          = 0521,
    KEY_NPAGE       = 0522,
    KEY_PPAGE       = 0523,
    KEY_ENTER       = 0527,
    KEY_A1          = 0534,
    KEY_A3          = 0535,
    KEY_B2          = 0536,
    KEY_C1          = 0537,
    KEY_C3          = 0540,
    KEY_BTAB        = 0541,
    KEY_END         = 0550,
    KEY_SDC         = 0577,
    KEY_SEND        = 0602,
    KEY_SHOME       = 0607,
    KEY_SIC         = 0610,
    KEY_SLEFT       = 0611,
    KEY_SNEXT       = 0614,
    KEY_SPREVIOUS   = 0616,
    KEY_SRIGHT      = 0622,
    KEY_SUSPEND     = 0627,
    KEY_MOUSE       = 0631,
    // Пользовательские возможности клавиатуры получают коды KEY_MAX, KEY_MAX + 1 и далее.
    KEY_MAX         = 0777,
};

} // пространство имён termkeys

// KeyTrie сопоставляет последовательности клавиш их кодам.

class KeyTrie
{
    struct Node
    {
        uchar ch;
        int child {-1};
        int sibling {-1};
        int value {0};
    };

    std::vector<Node> nodes;

    int findChild(int node, uchar ch) const noexcept;
    int findDefinition(int first, TStringView str) const noexcept;

public:

    KeyTrie() noexcept;

    // Навигация. Индексы узлов неотрицательны; -1 означает отсутствие узла.
    int root() const noexcept { return nodes[0].child; }
    int sibling(int node) const noexcept { return nodes[node].sibling; }
    int child(int node) const noexcept { return nodes[node].child; }
    int value(int node) const noexcept { return nodes[node].value; }
    uchar ch(int node) const noexcept { return nodes[node].ch; }

    // Поведение как у _nc_add_to_try() в Ncurses: если seq уже существует,
    // значение заменяется. Байт '\200' в seq обозначает '\0'. value должен
    // быть положительным.
    void insert(TStringView seq, int value) noexcept;

    // Поведение как у key_defined() в Ncurses: 0, если seq не определена;
    // -1, если она является префиксом других последовательностей; иначе
    // связанное с ней значение.
    int keyDefined(TStringView seq) const noexcept;
};

// Читает ввод аналогично wgetch() в Ncurses при включённом режиме
// клавиатуры: возвращает отдельные байты либо код клавиши при получении
// последовательности из terminfo. Как в Ncurses, unget возвращает байт
// или код в начало очереди ввода; возвращённые байты вновь участвуют в
// распознавании последовательностей.
//
//

class TerminalInputGetter final : public InputGetter
{
    enum { rawBufSize = 1024, fifoSize = 256 };
    enum { mouseIntervalMs = 166 }; // Аналог mouseinterval() из Ncurses.

    int fd;
    const KeyTrie &keyTrie;
    int escDelayMs;
    // Байты, прочитанные из терминала, но ещё не обработанные.
    size_t rawBegin {0}, rawEnd {0};
    uchar rawBuf[rawBufSize];
    // Очередь байтов и кодов клавиш, аналогичная FIFO в Ncurses.
    int fifo[fifoSize];
    size_t fifoHead {0}, fifoCount {0}, fifoPeek {0};

    int rawGet(int ms) noexcept;
    bool hasActivity(int ms) noexcept;
    bool fifoPushBack(int k) noexcept;
    void fifoPushFront(int k) noexcept;
    int &fifoAt(size_t i) noexcept;
    int fifoPopFront() noexcept;
    void fifoErase(size_t i) noexcept;
    int kgetch() noexcept;

public:

    // Таймаут, используемый get().
    int timeoutMs;

    TerminalInputGetter(int aFd, const KeyTrie &aKeyTrie, int aEscDelayMs, int aTimeoutMs) noexcept :
        fd(aFd),
        keyTrie(aKeyTrie),
        escDelayMs(aEscDelayMs),
        timeoutMs(aTimeoutMs)
    {
    }

    // Возвращает следующий байт или код клавиши либо -1, если за ms миллисекунд ввода
    // нет. 0 означает не ждать, отрицательное значение — ждать неограниченно.
    int get(int ms) noexcept;

    int get() noexcept override;
    void unget(int k) noexcept override;

    // Есть ли уже прочитанные из терминала, но ещё не использованные данные.
    //
    bool hasPending() const noexcept;
};

class TerminalInput final : public InputAdapter
{
    enum : char { KEY_ESC = '\x1B' };
    enum { readTimeoutMs = 10 };

    ConsoleCtl &con;
    TerminalDisplay &display;
    InputState &state;
    bool mouseEnabled;
    KeyTrie keyTrie;
    std::vector<const char *> extKeyNames;
    TerminalInputGetter in;

    int getChNb() noexcept;
    void detectAlt(int keys[4], bool &Alt) noexcept;
    void parsePrintableChar(TEvent &ev, int keys[4], int &num_keys) noexcept;
    void readUtf8Char(int keys[4], int &num_keys) noexcept;

    bool parseMouse(TEvent&) noexcept;

public:

    // con, display и state должны существовать дольше this.
    TerminalInput(ConsoleCtl &con, TerminalDisplay &display, InputState &state, bool mouse) noexcept;
    ~TerminalInput();

    bool getEvent(TEvent &ev) noexcept override;
    bool hasPendingEvents() noexcept override;

    // Строит таблицу последовательностей клавиш из terminfo аналогично
    // Ncurses. Стандартным возможностям назначаются коды клавиш Ncurses.
    // Пользовательским, например kDC5, назначаются KEY_MAX + n, где n —
    // индекс среди пользовательских строковых возможностей из extNames.
    //
    static void buildKeyTable( const TermInfo &termInfo, KeyTrie &keyTrie,
                               std::vector<const char *> &extNames ) noexcept;

    // Преобразует код клавиши в событие клавиатуры либо возвращает пустое
    // событие, если клавиша не поддерживается Turbo Vision.
    static KeyDownEvent keyFromCode(int code, const std::vector<const char *> &extNames) noexcept;
};

} // пространство имён tvision

#endif // _TV_UNIX

#endif // TVISION_TERMINP_H
