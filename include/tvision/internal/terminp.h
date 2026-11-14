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

// Key codes, with the same values as in Ncurses. Key codes are returned
// by 'TerminalInputGetter::get' for the key sequences described by the
// terminfo database.

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
    // User-defined key capabilities get the codes KEY_MAX, KEY_MAX + 1...
    KEY_MAX         = 0777,
};

} // namespace termkeys

// KeyTrie maps key sequences to key codes.

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

    // Navigation. Node indices are non-negative; -1 means 'none'.
    int root() const noexcept { return nodes[0].child; }
    int sibling(int node) const noexcept { return nodes[node].sibling; }
    int child(int node) const noexcept { return nodes[node].child; }
    int value(int node) const noexcept { return nodes[node].value; }
    uchar ch(int node) const noexcept { return nodes[node].ch; }

    // Same semantics as Ncurses' '_nc_add_to_try()': if 'seq' is already
    // present, its value is replaced. A byte '\200' in 'seq' stands for '\0'.
    // 'value' must be positive.
    void insert(TStringView seq, int value) noexcept;

    // Same semantics as Ncurses' 'key_defined()': returns 0 if 'seq' is not
    // defined, -1 if it is a prefix of other sequences, or otherwise the
    // value associated to it.
    int keyDefined(TStringView seq) const noexcept;
};

// Reads input from the terminal in the same way as Ncurses' 'wgetch()' does
// in keypad mode: it returns either single bytes or, when it receives one of
// the key sequences described by the terminfo database, a key code.
//
// Like in Ncurses, 'unget' puts back a byte or key code at the front of the
// input queue, and bytes put back are subject to key sequence recognition
// again.

class TerminalInputGetter final : public InputGetter
{
    enum { rawBufSize = 1024, fifoSize = 256 };
    enum { mouseIntervalMs = 166 }; // Like Ncurses' 'mouseinterval()'.

    int fd;
    const KeyTrie &keyTrie;
    int escDelayMs;
    // Bytes read from the terminal and not yet processed.
    size_t rawBegin {0}, rawEnd {0};
    uchar rawBuf[rawBufSize];
    // Input queue (bytes and key codes), like Ncurses' FIFO.
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

    // Timeout used by 'get()'.
    int timeoutMs;

    TerminalInputGetter(int aFd, const KeyTrie &aKeyTrie, int aEscDelayMs, int aTimeoutMs) noexcept :
        fd(aFd),
        keyTrie(aKeyTrie),
        escDelayMs(aEscDelayMs),
        timeoutMs(aTimeoutMs)
    {
    }

    // Returns the next byte or key code, or -1 if no input is received
    // within 'ms' milliseconds (0: do not wait; negative: wait indefinitely).
    int get(int ms) noexcept;

    int get() noexcept override;
    void unget(int k) noexcept override;

    // Whether there is input that has already been read from the terminal
    // but has not been consumed yet.
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

    // Lifetimes of 'con', 'display' and 'state' must exceed that of 'this'.
    TerminalInput(ConsoleCtl &con, TerminalDisplay &display, InputState &state, bool mouse) noexcept;
    ~TerminalInput();

    bool getEvent(TEvent &ev) noexcept override;
    bool hasPendingEvents() noexcept override;

    // Builds the table of key sequences described by the terminfo database,
    // in the same way as Ncurses. Standard key capabilities are mapped to
    // their Ncurses key codes. User-defined key capabilities (such as 'kDC5')
    // are mapped to 'KEY_MAX + n', where 'n' is their index among the
    // user-defined string capabilities, which are listed in 'extNames'.
    static void buildKeyTable( const TermInfo &termInfo, KeyTrie &keyTrie,
                               std::vector<const char *> &extNames ) noexcept;

    // Translates a key code into a key event, or returns an empty event if
    // the key is not supported by Turbo Vision.
    static KeyDownEvent keyFromCode(int code, const std::vector<const char *> &extNames) noexcept;
};

} // namespace tvision

#endif // _TV_UNIX

#endif // TVISION_TERMINP_H
