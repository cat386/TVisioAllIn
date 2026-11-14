#include <internal/terminp.h>

#ifdef _TV_UNIX

#define Uses_THardwareInfo
#define Uses_TKeys
#define Uses_TEvent
#include <tvision/tv.h>

#include <internal/termdisp.h>
#include <internal/terminfo.h>
#include <internal/getenv.h>
#include <internal/utf8.h>
#include <internal/conctl.h>
#include <internal/codepage.h>
#include <internal/constmap.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <string.h>

namespace tvision
{

using namespace termkeys;

/* Turbo Vision is designed to work with BIOS key codes. Mnemonics for some
 * key codes are defined in tkeys.h. Unless those are changed, it is
 * necessary to translate terminal keys to key codes. */

/* Turbo Vision stores key events in a KeyDownEvent struct, defined in
 * system.h. Its first field is a key code (16 bit), which can be decomposed
 * into the ASCII equivalent (lower byte) and a scan code (higher byte).
 * It has a second field with the state of the modifier keys, which can be
 * retrieved by performing a bit-wise AND with the kbShift, kbCtrlShift and
 * kbAltShift bitmasks. Turbo Vision expects this field to be filled even
 * if the key code is already named Shift/Ctrl/Alt+something. */

/* The support for key combinations is the following:
   - PrintScreen, Break are not likely to be captured by the terminal, but
     Ctrl+C could be used as a replacement of the Ctrl+Break interrupt.
   - Ctrl/Alt+F(n) don't work on the linux console and I strongly advice against
     using them.
   - Ctrl+Letter works, except for ^H, ^I, ^J and ^M, which have a special
     meaning.
   - Alt+Letter/Number seem to work quite well.
   - Ctrl+Backspace/Enter can't be recognized on terminal emulators.
   - Shift/Ctrl+Ins/Del/Home/End/PgDn/PgUp seem to work, too.
   - Arrow keys work, as well as combined with Shift, but Turbo Vision doesn't
     support Ctrl+Up/Down (EDIT: it now does).
   - Tab and Backtab are supported too, although the linux console confuses the
     latter with Alt+Tab.
   - Some other key combinations are supported on terminal but not in Turbo Vision.
 * Still, it's up to your luck that the terminal sends any of these
 * combinations in a recognizable way. */

static constexpr KeyDownEvent fromNonPrintableAscii[32] =
{
    {{'@'},         kbLeftCtrl, {'@'}, 1}, // ^@, Null
    {{kbCtrlA},     kbLeftCtrl},
    {{kbCtrlB},     kbLeftCtrl},
    {{kbCtrlC},     kbLeftCtrl},
    {{kbCtrlD},     kbLeftCtrl},
    {{kbCtrlE},     kbLeftCtrl},
    {{kbCtrlF},     kbLeftCtrl},
    {{kbCtrlG},     kbLeftCtrl},
    {{kbBack},      0}, // ^H, Backspace
    {{kbTab},       0}, // ^I, Tab
    {{kbEnter},     0}, // ^J, Line Feed
    {{kbCtrlK},     kbLeftCtrl},
    {{kbCtrlL},     kbLeftCtrl},
    {{kbEnter},     0}, // ^M, Carriage Return
    {{kbCtrlN},     kbLeftCtrl},
    {{kbCtrlO},     kbLeftCtrl},
    {{kbCtrlP},     kbLeftCtrl},
    {{kbCtrlQ},     kbLeftCtrl},
    {{kbCtrlR},     kbLeftCtrl},
    {{kbCtrlS},     kbLeftCtrl},
    {{kbCtrlT},     kbLeftCtrl},
    {{kbCtrlU},     kbLeftCtrl},
    {{kbCtrlV},     kbLeftCtrl},
    {{kbCtrlW},     kbLeftCtrl},
    {{kbCtrlX},     kbLeftCtrl},
    {{kbCtrlY},     kbLeftCtrl},
    {{kbCtrlZ},     kbLeftCtrl},
    {{kbEsc},       0}, // ^[, Escape
    {{'\\'},        kbLeftCtrl, {'\\'}, 1}, // ^\, File Separator
    {{']'},         kbLeftCtrl, {']'}, 1}, // ^], Group Separator
    {{'^'},         kbLeftCtrl, {'^'}, 1}, // ^^, Record Separator
    {{'_'},         kbLeftCtrl, {'_'}, 1}, // ^_, Unit Separator
};

static const const_unordered_map<ushort, KeyDownEvent> fromCursesKeyCode =
{
    { KEY_DOWN,         {{kbDown},      0}          },
    { KEY_UP,           {{kbUp},        0}          },
    { KEY_LEFT,         {{kbLeft},      0}          },
    { KEY_RIGHT,        {{kbRight},     0}          },
    { KEY_HOME,         {{kbHome},      0}          },
    { KEY_BACKSPACE,    {{kbBack},      0}          },
    { KEY_DC,           {{kbDel},       0}          },
    { KEY_IC,           {{kbIns},       0}          },
    { KEY_SF,           {{kbDown},      kbShift}    },
    { KEY_SR,           {{kbUp},        kbShift}    },
    { KEY_NPAGE,        {{kbPgDn},      0}          },
    { KEY_PPAGE,        {{kbPgUp},      0}          },
    { KEY_ENTER,        {{kbEnter},     0}          },
    { KEY_BTAB,         {{kbShiftTab},  kbShift}    },
    { KEY_END,          {{kbEnd},       0}          },
    { KEY_SDC,          {{kbShiftDel},  kbShift}    },
    { KEY_SEND,         {{kbEnd},       kbShift}    },
    { KEY_SHOME,        {{kbHome},      kbShift}    },
    { KEY_SIC,          {{kbShiftIns},  kbShift}    },
    { KEY_SLEFT,        {{kbLeft},      kbShift}    },
    { KEY_SRIGHT,       {{kbRight},     kbShift}    },
    { KEY_SUSPEND,      {{kbCtrlZ},     kbLeftCtrl} },
    // Avoid using these, as they are reserved by the Linux console.
    { KEY_SPREVIOUS,    {{kbPgUp},      kbShift}    },
    { KEY_SNEXT,        {{kbPgDn},      kbShift}    },
    // Keypad
    { KEY_A1,           {{kbHome},      0}          },
    { KEY_A3,           {{kbPgUp},      0}          },
    { KEY_C1,           {{kbEnd},       0}          },
    { KEY_C3,           {{kbPgDn},      0}          },
    // Function keys F1-F12
    { KEY_F0 + 1,       {{kbF1},        0}          },
    { KEY_F0 + 2,       {{kbF2},        0}          },
    { KEY_F0 + 3,       {{kbF3},        0}          },
    { KEY_F0 + 4,       {{kbF4},        0}          },
    { KEY_F0 + 5,       {{kbF5},        0}          },
    { KEY_F0 + 6,       {{kbF6},        0}          },
    { KEY_F0 + 7,       {{kbF7},        0}          },
    { KEY_F0 + 8,       {{kbF8},        0}          },
    { KEY_F0 + 9,       {{kbF9},        0}          },
    { KEY_F0 + 10,      {{kbF10},       0}          },
    { KEY_F0 + 11,      {{kbF11},       0}          },
    { KEY_F0 + 12,      {{kbF12},       0}          },
    // Shift+F1-F12
    { KEY_F0 + 13,      {{kbShiftF1},   kbShift}    },
    { KEY_F0 + 14,      {{kbShiftF2},   kbShift}    },
    { KEY_F0 + 15,      {{kbShiftF3},   kbShift}    },
    { KEY_F0 + 16,      {{kbShiftF4},   kbShift}    },
    { KEY_F0 + 17,      {{kbShiftF5},   kbShift}    },
    { KEY_F0 + 18,      {{kbShiftF6},   kbShift}    },
    { KEY_F0 + 19,      {{kbShiftF7},   kbShift}    },
    { KEY_F0 + 20,      {{kbShiftF8},   kbShift}    },
    { KEY_F0 + 21,      {{kbShiftF9},   kbShift}    },
    { KEY_F0 + 22,      {{kbShiftF10},  kbShift}    },
    { KEY_F0 + 23,      {{kbShiftF11},  kbShift}    },
    { KEY_F0 + 24,      {{kbShiftF12},  kbShift}    },
    /* Linux console support for function keys ends here, so please
     * avoid using any of the following: */
    // Ctrl+F1-F12
    { KEY_F0 + 25,      {{kbCtrlF1},    kbLeftCtrl} },
    { KEY_F0 + 26,      {{kbCtrlF2},    kbLeftCtrl} },
    { KEY_F0 + 27,      {{kbCtrlF3},    kbLeftCtrl} },
    { KEY_F0 + 28,      {{kbCtrlF4},    kbLeftCtrl} },
    { KEY_F0 + 29,      {{kbCtrlF5},    kbLeftCtrl} },
    { KEY_F0 + 30,      {{kbCtrlF6},    kbLeftCtrl} },
    { KEY_F0 + 31,      {{kbCtrlF7},    kbLeftCtrl} },
    { KEY_F0 + 32,      {{kbCtrlF8},    kbLeftCtrl} },
    { KEY_F0 + 33,      {{kbCtrlF9},    kbLeftCtrl} },
    { KEY_F0 + 34,      {{kbCtrlF10},   kbLeftCtrl} },
    { KEY_F0 + 35,      {{kbCtrlF11},   kbLeftCtrl} },
    { KEY_F0 + 36,      {{kbCtrlF12},   kbLeftCtrl} },
    // Ctrl+Shift+F1-12
    { KEY_F0 + 37,      {{kbCtrlF1},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 38,      {{kbCtrlF2},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 39,      {{kbCtrlF3},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 40,      {{kbCtrlF4},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 41,      {{kbCtrlF5},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 42,      {{kbCtrlF6},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 43,      {{kbCtrlF7},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 44,      {{kbCtrlF8},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 45,      {{kbCtrlF9},    kbShift | kbLeftCtrl}},
    { KEY_F0 + 46,      {{kbCtrlF10},   kbShift | kbLeftCtrl}},
    { KEY_F0 + 47,      {{kbCtrlF11},   kbShift | kbLeftCtrl}},
    { KEY_F0 + 48,      {{kbCtrlF12},   kbShift | kbLeftCtrl}},
    // Alt+F1-F12
    { KEY_F0 + 49,      {{kbAltF1},     kbLeftAlt}  },
    { KEY_F0 + 50,      {{kbAltF2},     kbLeftAlt}  },
    { KEY_F0 + 51,      {{kbAltF3},     kbLeftAlt}  },
    { KEY_F0 + 52,      {{kbAltF4},     kbLeftAlt}  },
    { KEY_F0 + 53,      {{kbAltF5},     kbLeftAlt}  },
    { KEY_F0 + 54,      {{kbAltF6},     kbLeftAlt}  },
    { KEY_F0 + 55,      {{kbAltF7},     kbLeftAlt}  },
    { KEY_F0 + 56,      {{kbAltF8},     kbLeftAlt}  },
    { KEY_F0 + 57,      {{kbAltF9},     kbLeftAlt}  },
    { KEY_F0 + 58,      {{kbAltF10},    kbLeftAlt}  },
    { KEY_F0 + 59,      {{kbAltF11},    kbLeftAlt}  },
    { KEY_F0 + 60,      {{kbAltF12},    kbLeftAlt}  }
};

static const auto fromCursesHighKey =
    const_unordered_map<uint64_t, KeyDownEvent>::with_string_keys({
    /* These keys are identified by name. The int value is not known
     * at compilation time. */
    { "kDC3",       {{kbAltDel},        kbLeftAlt} },
    { "kEND3",      {{kbAltEnd},        kbLeftAlt} },
    { "kHOM3",      {{kbAltHome},       kbLeftAlt} },
    { "kIC3",       {{kbAltIns},        kbLeftAlt} },
    { "kLFT3",      {{kbAltLeft},       kbLeftAlt} },
    { "kNXT3",      {{kbAltPgDn},       kbLeftAlt} },
    { "kPRV3",      {{kbAltPgUp},       kbLeftAlt} },
    { "kRIT3",      {{kbAltRight},      kbLeftAlt} },
    { "kUP3",       {{kbAltUp},         kbLeftAlt} },
    { "kDN3",       {{kbAltDown},       kbLeftAlt} },
    { "kDC4",       {{kbAltDel},        kbShift | kbLeftAlt} },
    { "kEND4",      {{kbAltEnd},        kbShift | kbLeftAlt} },
    { "kHOM4",      {{kbAltHome},       kbShift | kbLeftAlt} },
    { "kIC4",       {{kbAltIns},        kbShift | kbLeftAlt} },
    { "kLFT4",      {{kbAltLeft},       kbShift | kbLeftAlt} },
    { "kNXT4",      {{kbAltPgDn},       kbShift | kbLeftAlt} },
    { "kPRV4",      {{kbAltPgUp},       kbShift | kbLeftAlt} },
    { "kRIT4",      {{kbAltRight},      kbShift | kbLeftAlt} },
    { "kUP4",       {{kbAltUp},         kbShift | kbLeftAlt} },
    { "kDN4",       {{kbAltDown},       kbShift | kbLeftAlt} },
    { "kDC5",       {{kbCtrlDel},       kbLeftCtrl} },
    { "kEND5",      {{kbCtrlEnd},       kbLeftCtrl} },
    { "kHOM5",      {{kbCtrlHome},      kbLeftCtrl} },
    { "kIC5",       {{kbCtrlIns},       kbLeftCtrl} },
    { "kLFT5",      {{kbCtrlLeft},      kbLeftCtrl} },
    { "kNXT5",      {{kbCtrlPgDn},      kbLeftCtrl} },
    { "kPRV5",      {{kbCtrlPgUp},      kbLeftCtrl} },
    { "kRIT5",      {{kbCtrlRight},     kbLeftCtrl} },
    { "kUP5",       {{kbCtrlUp},        kbLeftCtrl} },
    { "kDN5",       {{kbCtrlDown},      kbLeftCtrl} },
    { "kDC6",       {{kbCtrlDel},       kbLeftCtrl | kbShift} },
    { "kEND6",      {{kbCtrlEnd},       kbLeftCtrl | kbShift} },
    { "kHOM6",      {{kbCtrlHome},      kbLeftCtrl | kbShift} },
    { "kIC6",       {{kbCtrlIns},       kbLeftCtrl | kbShift} },
    { "kLFT6",      {{kbCtrlLeft},      kbLeftCtrl | kbShift} },
    { "kNXT6",      {{kbCtrlPgDn},      kbLeftCtrl | kbShift} },
    { "kPRV6",      {{kbCtrlPgUp},      kbLeftCtrl | kbShift} },
    { "kRIT6",      {{kbCtrlRight},     kbLeftCtrl | kbShift} },
    { "kUP6",       {{kbCtrlUp},        kbLeftCtrl | kbShift} },
    { "kDN6",       {{kbCtrlDown},      kbLeftCtrl | kbShift} },
    { "kDC7",       {{kbAltDel},        kbLeftCtrl | kbLeftAlt} }, // Please do not attempt this one
    { "kEND7",      {{kbAltEnd},        kbLeftCtrl | kbLeftAlt} },
    { "kHOM7",      {{kbAltHome},       kbLeftCtrl | kbLeftAlt} },
    { "kIC7",       {{kbAltIns},        kbLeftCtrl | kbLeftAlt} },
    { "kLFT7",      {{kbAltLeft},       kbLeftCtrl | kbLeftAlt} },
    { "kNXT7",      {{kbAltPgDn},       kbLeftCtrl | kbLeftAlt} },
    { "kPRV7",      {{kbAltPgUp},       kbLeftCtrl | kbLeftAlt} },
    { "kRIT7",      {{kbAltRight},      kbLeftCtrl | kbLeftAlt} },
    { "kUP7",       {{kbAltUp},         kbLeftCtrl | kbLeftAlt} },
    { "kDN7",       {{kbAltDown},       kbLeftCtrl | kbLeftAlt} },
    { "kpCMA",      {{'+'},             0, {'+'}, 1} },
    { "kpADD",      {{'+'},             0, {'+'}, 1} },
    { "kpSUB",      {{'-'},             0, {'-'}, 1} },
    { "kpMUL",      {{'*'},             0, {'*'}, 1} },
    { "kpDIV",      {{'/'},             0, {'/'}, 1} },
    { "kpZRO",      {{'0'},             0, {'0'}, 1} },
    { "kpDOT",      {{'.'},             0, {'.'}, 1} },
    { "ka2",        {{kbUp},            0} },
    { "kb1",        {{kbLeft},          0} },
    { "kb3",        {{kbRight},         0} },
    { "kc2",        {{kbDown},          0} },
});

// Ncurses key codes (KEY_*) of the standard key capabilities, in the order
// in which Ncurses adds them to its key table (sorted by the names of the
// KEY_* constants). When several capabilities share the same sequence,
// the last one in this list wins.
struct StandardKey
{
    const char *name;
    int code;
};

static const StandardKey standardKeys[] =
{
    {"ka1", 0534}, {"ka3", 0535}, {"kb2", 0536}, {"kbs", 0407},
    {"kbeg", 0542}, {"kcbt", 0541}, {"kc1", 0537}, {"kc3", 0540},
    {"kcan", 0543}, {"ktbc", 0526}, {"kclr", 0515}, {"kclo", 0544},
    {"kcmd", 0545}, {"kcpy", 0546}, {"kcrt", 0547}, {"kctab", 0525},
    {"kdch1", 0512}, {"kdl1", 0510}, {"kcud1", 0402}, {"krmir", 0514},
    {"kend", 0550}, {"kent", 0527}, {"kel", 0517}, {"ked", 0516},
    {"kext", 0551}, {"kf0", 0410}, {"kf1", 0411}, {"kf10", 0422},
    {"kf11", 0423}, {"kf12", 0424}, {"kf13", 0425}, {"kf14", 0426},
    {"kf15", 0427}, {"kf16", 0430}, {"kf17", 0431}, {"kf18", 0432},
    {"kf19", 0433}, {"kf2", 0412}, {"kf20", 0434}, {"kf21", 0435},
    {"kf22", 0436}, {"kf23", 0437}, {"kf24", 0440}, {"kf25", 0441},
    {"kf26", 0442}, {"kf27", 0443}, {"kf28", 0444}, {"kf29", 0445},
    {"kf3", 0413}, {"kf30", 0446}, {"kf31", 0447}, {"kf32", 0450},
    {"kf33", 0451}, {"kf34", 0452}, {"kf35", 0453}, {"kf36", 0454},
    {"kf37", 0455}, {"kf38", 0456}, {"kf39", 0457}, {"kf4", 0414},
    {"kf40", 0460}, {"kf41", 0461}, {"kf42", 0462}, {"kf43", 0463},
    {"kf44", 0464}, {"kf45", 0465}, {"kf46", 0466}, {"kf47", 0467},
    {"kf48", 0470}, {"kf49", 0471}, {"kf5", 0415}, {"kf50", 0472},
    {"kf51", 0473}, {"kf52", 0474}, {"kf53", 0475}, {"kf54", 0476},
    {"kf55", 0477}, {"kf56", 0500}, {"kf57", 0501}, {"kf58", 0502},
    {"kf59", 0503}, {"kf6", 0416}, {"kf60", 0504}, {"kf61", 0505},
    {"kf62", 0506}, {"kf63", 0507}, {"kf7", 0417}, {"kf8", 0420},
    {"kf9", 0421}, {"kfnd", 0552}, {"khlp", 0553}, {"khome", 0406},
    {"kich1", 0513}, {"kil1", 0511}, {"kcub1", 0404}, {"kll", 0533},
    {"kmrk", 0554}, {"kmsg", 0555}, {"kmous", 0631}, {"kmov", 0556},
    {"knxt", 0557}, {"knp", 0522}, {"kopn", 0560}, {"kopt", 0561},
    {"kpp", 0523}, {"kprv", 0562}, {"kprt", 0532}, {"krdo", 0563},
    {"kref", 0564}, {"krfr", 0565}, {"krpl", 0566}, {"krst", 0567},
    {"kres", 0570}, {"kcuf1", 0405}, {"ksav", 0571}, {"kBEG", 0572},
    {"kCAN", 0573}, {"kCMD", 0574}, {"kCPY", 0575}, {"kCRT", 0576},
    {"kDC", 0577}, {"kDL", 0600}, {"kslt", 0601}, {"kEND", 0602},
    {"kEOL", 0603}, {"kEXT", 0604}, {"kind", 0520}, {"kFND", 0605},
    {"kHLP", 0606}, {"kHOM", 0607}, {"kIC", 0610}, {"kLFT", 0611},
    {"kMSG", 0612}, {"kMOV", 0613}, {"kNXT", 0614}, {"kOPT", 0615},
    {"kPRV", 0616}, {"kPRT", 0617}, {"kri", 0521}, {"kRDO", 0620},
    {"kRPL", 0621}, {"kRIT", 0622}, {"kRES", 0623}, {"kSAV", 0624},
    {"kSPD", 0625}, {"khts", 0524}, {"kUND", 0626}, {"kspd", 0627},
    {"kund", 0630}, {"kcuu1", 0403},
};

/////////////////////////////////////////////////////////////////////////
// KeyTrie

KeyTrie::KeyTrie() noexcept :
    nodes(1) // Dummy root node.
{
}

int KeyTrie::findChild(int node, uchar ch) const noexcept
{
    for (int i = nodes[node].child; i != -1; i = nodes[i].sibling)
        if (nodes[i].ch == ch)
            return i;
    return -1;
}

void KeyTrie::insert(TStringView seq, int value) noexcept
{
    if (seq.empty() || value <= 0)
        return;
    int node = 0;
    for (char c : seq)
    {
        uchar ch = (uchar) c == 128 ? '\0' : (uchar) c;
        int next = findChild(node, ch);
        if (next == -1)
        {
            // Like Ncurses, new nodes are appended after their siblings.
            next = (int) nodes.size();
            nodes.emplace_back();
            nodes[next].ch = ch;
            int *link = &nodes[node].child;
            while (*link != -1)
                link = &nodes[*link].sibling;
            *link = next;
        }
        node = next;
    }
    nodes[node].value = value;
}

int KeyTrie::findDefinition(int ptr, TStringView str) const noexcept
// Port of Ncurses' 'find_definition()'.
{
    enum { OK = 0, ERR = -1 };
    int result = OK;
    if (!str.empty())
    {
        for (; ptr != -1; ptr = nodes[ptr].sibling)
        {
            if ((uchar) str[0] == nodes[ptr].ch)
            {
                if (str.size() == 1 && nodes[ptr].child != -1)
                    result = ERR;
                else if ((result = findDefinition(nodes[ptr].child, str.substr(1))) == OK)
                    result = nodes[ptr].value;
                else if (str.size() == 1)
                    result = ERR;
            }
            if (result != OK)
                break;
        }
    }
    return result;
}

int KeyTrie::keyDefined(TStringView seq) const noexcept
{
    return findDefinition(nodes[0].child, seq);
}

/////////////////////////////////////////////////////////////////////////
// TerminalInputGetter
//
// The input queue works like Ncurses' FIFO: the items between the front of
// the queue and the 'peek' position ('fifoPeek' items) have already been
// examined ("cooked") and are returned as they are. The rest ("raw") are
// subject to key sequence recognition. Items put back with 'unget' are
// always cooked.

int TerminalInputGetter::rawGet(int ms) noexcept
{
    if (rawBegin < rawEnd)
        return rawBuf[rawBegin++];
    if (fd < 0 || !hasActivity(ms))
        return -1;
    ssize_t r;
    while ((r = read(fd, rawBuf, rawBufSize)) == -1 && errno == EINTR);
    if (r <= 0)
        return -1;
    rawBegin = 1;
    rawEnd = (size_t) r;
    return rawBuf[0];
}

bool TerminalInputGetter::hasActivity(int ms) noexcept
// Whether there are bytes to be read from the terminal, waiting at most
// 'ms' milliseconds. Like Ncurses' 'check_mouse_activity()'.
{
    if (rawBegin < rawEnd)
        return true;
    if (fd < 0)
        return false;
    // We use 'select' instead of 'poll' because 'poll' does not support
    // terminal devices on macOS.
    if (fd < FD_SETSIZE)
    {
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(fd, &readFds);
        struct timeval timeout;
        timeout.tv_sec = ms / 1000;
        timeout.tv_usec = (ms % 1000)*1000;
        int res;
        while ( (res = select(fd + 1, &readFds, nullptr, nullptr, ms < 0 ? nullptr : &timeout)) == -1
                && errno == EINTR );
        return res > 0;
    }
    return true;
}

inline int &TerminalInputGetter::fifoAt(size_t i) noexcept
{
    return fifo[(fifoHead + i) % fifoSize];
}

bool TerminalInputGetter::fifoPushBack(int k) noexcept
{
    if (fifoCount < fifoSize)
    {
        fifoAt(fifoCount++) = k;
        return true;
    }
    return false;
}

void TerminalInputGetter::fifoPushFront(int k) noexcept
{
    if (fifoCount < fifoSize)
    {
        fifoHead = (fifoHead + fifoSize - 1) % fifoSize;
        fifo[fifoHead] = k;
        ++fifoCount;
    }
}

int TerminalInputGetter::fifoPopFront() noexcept
{
    if (fifoCount == 0)
        return -1;
    int k = fifo[fifoHead];
    fifoHead = (fifoHead + 1) % fifoSize;
    --fifoCount;
    if (fifoPeek > 0)
        --fifoPeek;
    return k;
}

void TerminalInputGetter::fifoErase(size_t i) noexcept
{
    if (i < fifoCount)
    {
        for (size_t j = i; j + 1 < fifoCount; ++j)
            fifoAt(j) = fifoAt(j + 1);
        --fifoCount;
    }
}

int TerminalInputGetter::kgetch() noexcept
// Port of Ncurses' 'kgetch()': reads a key sequence and returns its key code,
// or the first byte if it does not form a key sequence.
{
    int node = keyTrie.root();
    for (;;)
    {
        if (fifoPeek > 0 && fifoAt(0) >= KEY_MIN)
            break;
        else if (fifoPeek >= fifoCount)
        {
            // No raw items: read more input.
            int k = rawGet(0);
            if (k == -1 || !fifoPushBack(k))
            {
                fifoPeek = 0; // The items stay uninterpreted.
                return -1;
            }
        }

        int ch = fifoAt(fifoPeek++);
        if (ch >= KEY_MIN)
        {
            // A key code which was put back in the middle of the queue.
            fifoPeek = 0;
            fifoErase(fifoCount - 1);
            return ch;
        }

        while (node != -1 && keyTrie.ch(node) != (uchar) ch)
            node = keyTrie.sibling(node);
        if (node == -1)
            break;
        if (keyTrie.value(node) != 0)
        {
            // Sequence terminated: remove it from the queue.
            int value = keyTrie.value(node);
            size_t n = fifoPeek;
            fifoPeek = 0;
            while (n-- > 0)
                fifoPopFront();
            return value;
        }
        node = keyTrie.child(node);

        if (fifoPeek >= fifoCount && !hasActivity(escDelayMs))
            break; // Ran out of time.
    }
    fifoPeek = 0;
    int ch = fifoPopFront();
    fifoPeek = 0;
    return ch;
}

int TerminalInputGetter::get(int ms) noexcept
// Port of Ncurses' 'wgetch()' in keypad mode.
{
    if (fifoPeek > 0)
        return fifoPopFront();
    if (fifoCount == 0 && !hasActivity(ms))
        return -1;

    // Like Ncurses, accumulate mouse events while they keep arriving. Since
    // we don't let Ncurses' mouse driver handle them, this just means that
    // after a mouse sequence ('kmous'), we wait for some more input and put
    // back the next item so that the mouse event can be parsed.
    int runcount = 0;
    int ch;
    do
    {
        ch = kgetch();
        if (ch == KEY_MOUSE)
            ++runcount;
    } while (ch == KEY_MOUSE && hasActivity(mouseIntervalMs));

    if (runcount > 0 && ch != KEY_MOUSE)
    {
        unget(ch);
        ch = KEY_MOUSE;
    }
    return ch;
}

int TerminalInputGetter::get() noexcept
{
    return get(timeoutMs);
}

void TerminalInputGetter::unget(int k) noexcept
// Port of Ncurses' 'ungetch()'.
{
    if (k == -1 || fifoCount >= fifoSize)
        return;
    bool wasEmpty = fifoCount == 0;
    fifoPushFront(k);
    if (wasEmpty)
        fifoPeek = fifoCount; // No raw items.
    else
        ++fifoPeek;
}

bool TerminalInputGetter::hasPending() const noexcept
{
    return fifoCount > 0 || rawBegin < rawEnd;
}

/////////////////////////////////////////////////////////////////////////
// TerminalInput

void TerminalInput::buildKeyTable( const TermInfo &termInfo, KeyTrie &keyTrie,
                                   std::vector<const char *> &extNames ) noexcept
{
    // Same as Ncurses' '_nc_init_keytry()': first, the standard key
    // capabilities are added in the order of 'standardKeys' (a later one
    // replaces an earlier one with the same sequence). Then, the user-defined
    // capabilities whose name begins with 'k' are added unless their sequence
    // is already defined.
    for (const auto &key : standardKeys)
        if (const char *value = termInfo.getStr(key.name))
            keyTrie.insert(value, key.code);
    extNames.clear();
    for (const auto &cap : termInfo.strs)
        if (!TermInfo::isStandardStrCap(cap.name.c_str()))
        {
            int code = KEY_MAX + (int) extNames.size();
            extNames.push_back(cap.name.c_str());
            TStringView value(cap.value.data(), cap.value.size());
            if ( cap.name[0] == 'k' && !value.empty() &&
                 keyTrie.keyDefined(value) == 0 )
                keyTrie.insert(value, code);
        }
}

KeyDownEvent TerminalInput::keyFromCode(int code, const std::vector<const char *> &extNames) noexcept
{
    if (KEY_MIN < code && code < KEY_MAX)
        return fromCursesKeyCode[code];
    // Note: Turbo Vision used to ignore the first user-defined key
    // (whose code is KEY_MAX) because of an off-by-one error.
    if (KEY_MAX <= code && size_t(code - KEY_MAX) < extNames.size())
        return fromCursesHighKey[extNames[code - KEY_MAX]];
    return {};
}

TerminalInput::TerminalInput( ConsoleCtl &aCon, TerminalDisplay &aDisplay,
                              InputState &aState, bool mouse ) noexcept :
    InputAdapter(aCon.in()),
    con(aCon),
    display(aDisplay),
    state(aState),
    mouseEnabled(mouse),
    // Do not delay too much on ESC key presses, as the Alt modifier works well
    // in most modern terminals. Still, this delay helps distinguish special key
    // sequences.
    // Make reads practically non-blocking. Some terminals may feed input slowly.
    // Note that we only risk blocking when reading multibyte characters
    // or parsing escape sequences.
    in(aCon.in(), keyTrie, getEnv<int>("ESCDELAY", 10), readTimeoutMs)
{
    buildKeyTable(display.termInfo, keyTrie, extKeyNames);
    // Capture all keyboard input, without echo and without translating CR.
    display.setRawInputMode();
    // Allow capturing function keys.
    display.setKeypadMode(true);

    TermIO::keyModsOn(con);
    if (mouseEnabled)
        TermIO::mouseOn(con);
}

TerminalInput::~TerminalInput()
{
    if (mouseEnabled)
        TermIO::mouseOff(con);
    TermIO::keyModsOff(con);
    TermIO::consumeUnprocessedInput(con, in, state);
}

int TerminalInput::getChNb() noexcept
{
    return in.get(0);
}

bool TerminalInput::hasPendingEvents() noexcept
{
    return in.hasPending();
}

bool TerminalInput::getEvent(TEvent &ev) noexcept
{
    GetChBuf buf(in);
    switch (TermIO::parseEvent(buf, ev, state))
    {
        case Rejected: buf.reject(); break;
        case Accepted: return true;
        case Ignored: return false;
    }

    int k = in.get();

    if (k == KEY_MOUSE)
        return parseMouse(ev);

    if (k != -1)
    {
        // A Unicode character might be composed of up to 4 UTF-8 bytes.
        int keys[4] = {k}, num_keys = 1;

        ev.what = evKeyDown;
        bool Alt = false;

        if (keys[0] == KEY_ESC)
            detectAlt(keys, Alt);

        if ((uint) keys[0] < 32)
            ev.keyDown = fromNonPrintableAscii[keys[0]];
        else if (keys[0] == 127)
            ev.keyDown = {{kbBack}, 0}; // ^?, Delete
        else if (KEY_MIN < keys[0])
            ev.keyDown = keyFromCode(keys[0], extKeyNames);

        // If it hasn't been transformed by any of the previous tables,
        // and it's not a key code, treat it like a printable character.
        if (ev.keyDown.keyCode == kbNoKey && keys[0] < KEY_MIN)
            parsePrintableChar(ev, keys, num_keys);

        if (Alt)
        {
            ev.keyDown.controlKeyState |= kbLeftAlt;
            TermIO::normalizeKey(ev.keyDown);
        }
        if (state.bracketedPaste)
            ev.keyDown.controlKeyState |= kbPaste;

        return ev.keyDown.keyCode != kbNoKey || ev.keyDown.textLength;
    }
    return false;
}

void TerminalInput::detectAlt(int keys[4], bool &Alt) noexcept
{
/* Alt+Key combinations begin with the character ESC. To tell the difference,
 * we check if another character has been received. If it has, we consider this
 * an Alt+Key combination. Of course, many other things sent by the terminal
 * begin with ESC, but most of them have already been identified. */
    int k = getChNb();
    if (k != -1)
    {
        keys[0] = k;
        Alt = true;
    }
}

void TerminalInput::parsePrintableChar(TEvent &ev, int keys[4], int &num_keys) noexcept
{
    // Read any possible remaining bytes.
    readUtf8Char(keys, num_keys);
    for (int i = 0; i < num_keys; ++i)
        ev.keyDown.text[i] = (char) keys[i];
    ev.keyDown.textLength = (uchar) num_keys;
    // If we are lucky enough, the character will be representable in
    // the active codepage.
    ev.keyDown.charScan.charCode = CpTranslator::fromUtf8({ev.keyDown.text, size_t(num_keys)});
    // Prevent text from triggering Ctrl+Key shortcuts.
    if (ev.keyDown.keyCode <= kbCtrlZ)
        ev.keyDown.keyCode = kbNoKey;
}

void TerminalInput::readUtf8Char(int keys[4], int &num_keys) noexcept
{
/* Unicode characters are sent by the terminal byte by byte. To read one, we
 * have to predict the number of bytes it is composed of, then read as many. */
    num_keys += Utf8BytesLeft((char) keys[0]);
    for (int i = 1; i < num_keys; ++i)
        if ((keys[i] = in.get()) == -1)
        {
            num_keys = i;
            break;
        }
}

bool TerminalInput::parseMouse(TEvent &ev) noexcept
// Pre: the sequence of the 'kmous' capability has just been read.
{
    // The 'kmous' capability is either "\x1B[M" or "\x1B[<", but we don't know
    // which one was read. We could query terminal capabilities to deduce it,
    // but it is also possible to just follow a trial and error approach.
    // 'parseSGRMouse' is more likely to fail, so try it first.
    for (auto &parseMouse : {TermIO::parseSGRMouse,
                             TermIO::parseX10Mouse})
    {
        GetChBuf buf(in);
        switch (parseMouse(buf, ev, state))
        {
            case Rejected: buf.reject(); break;
            case Accepted: return true;
            case Ignored: return false;
        }
    }
    return false;
}

} // namespace tvision

#endif // _TV_UNIX
