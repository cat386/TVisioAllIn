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

/*
 * Turbo Vision рассчитан на коды клавиш BIOS. Для некоторых кодов в tkeys.h определены мнемоники. Пока они не изменены, клавиши терминала нужно преобразовывать в эти коды.
 */

/*
 * Turbo Vision хранит событие клавиатуры в KeyDownEvent из system.h. Первое
 * поле — 16-битный код: младший байт содержит ASCII-эквивалент, старший —
 * скан-код. Второе поле содержит состояние модификаторов, проверяемое
 * побитовым И с kbShift, kbCtrlShift и kbAltShift. Поле должно быть
 * заполнено, даже если название кода уже включает Shift/Ctrl/Alt.
 */

/*
 * Поддержка сочетаний: PrintScreen и Break терминал обычно не передаёт,
 * но Ctrl+C может заменить прерывание Ctrl+Break. Ctrl/Alt+F(n) не
 * работают в консоли Linux, использовать их настоятельно не
 * рекомендуется. Ctrl+буква работает, кроме ^H, ^I, ^J и ^M с особым
 * значением. Alt+буква/цифра обычно работают. Ctrl+Backspace/Enter в
 * эмуляторах не распознаются. Shift/Ctrl+Ins/Del/Home/End/PgDn/PgUp
 * обычно работают. Стрелки и Shift+стрелки работают; Ctrl+Up/Down
 * изначально не поддерживались Turbo Vision, но теперь поддерживаются.
 * Tab и Backtab поддерживаются, хотя консоль Linux путает Backtab с
 * Alt+Tab. Некоторые сочетания доступны терминалу, но не Turbo Vision.
 * Возможность их распознавания зависит от того, какую последовательность
 * передаёт терминал.
 *
 *
 *
 */

static constexpr KeyDownEvent fromNonPrintableAscii[32] =
{
    {{'@'},         kbLeftCtrl, {'@'}, 1}, // ^@, нулевой символ.
    {{kbCtrlA},     kbLeftCtrl},
    {{kbCtrlB},     kbLeftCtrl},
    {{kbCtrlC},     kbLeftCtrl},
    {{kbCtrlD},     kbLeftCtrl},
    {{kbCtrlE},     kbLeftCtrl},
    {{kbCtrlF},     kbLeftCtrl},
    {{kbCtrlG},     kbLeftCtrl},
    {{kbBack},      0}, // ^H, Backspace.
    {{kbTab},       0}, // ^I, Tab.
    {{kbEnter},     0}, // ^J, перевод строки.
    {{kbCtrlK},     kbLeftCtrl},
    {{kbCtrlL},     kbLeftCtrl},
    {{kbEnter},     0}, // ^M, возврат каретки.
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
    {{kbEsc},       0}, // ^[, Escape.
    {{'\\'},        kbLeftCtrl, {'\\'}, 1}, // ^\, разделитель файлов.
    {{']'},         kbLeftCtrl, {']'}, 1}, // ^], разделитель групп.
    {{'^'},         kbLeftCtrl, {'^'}, 1}, // ^^, разделитель записей.
    {{'_'},         kbLeftCtrl, {'_'}, 1}, // ^_, разделитель элементов.
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
    // Не используйте эти значения: они зарезервированы консолью Linux.
    { KEY_SPREVIOUS,    {{kbPgUp},      kbShift}    },
    { KEY_SNEXT,        {{kbPgDn},      kbShift}    },
    // Цифровая клавиатура.
    { KEY_A1,           {{kbHome},      0}          },
    { KEY_A3,           {{kbPgUp},      0}          },
    { KEY_C1,           {{kbEnd},       0}          },
    { KEY_C3,           {{kbPgDn},      0}          },
    // Функциональные клавиши F1–F12.
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
    // Shift+F1–F12.
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
    /* На этом поддержка функциональных сочетаний консоли Linux заканчивается; избегайте следующих:
 */
    // Ctrl+F1–F12.
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
    // Ctrl+Shift+F1–F12.
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
    // Alt+F1–F12.
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
    /* Эти клавиши определяются по имени; целое значение неизвестно при компиляции.
 */
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
    { "kDC7",       {{kbAltDel},        kbLeftCtrl | kbLeftAlt} }, // Не пытайтесь использовать это сочетание.
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

// Коды Ncurses KEY_* для стандартных возможностей клавиатуры в порядке
// добавления в таблицу Ncurses: по именам констант KEY_*. Если несколько
// возможностей имеют одинаковую последовательность, приоритет у
// последней в списке.
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
    nodes(1) // Фиктивный корневой узел.
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
            // Как в Ncurses, новые узлы добавляются после соседних.
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
// Перенос find_definition() из Ncurses.
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

// TerminalInputGetter. Очередь ввода устроена как FIFO Ncurses: элементы
// от начала до позиции peek, числом fifoPeek, уже обработаны и
// возвращаются без изменений. Остальные, необработанные, участвуют в
// распознавании последовательностей клавиш. Возвращённые unget элементы
// всегда считаются обработанными.
//
//
//

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
// Проверяет наличие байтов для чтения из терминала, ожидая не более ms
// миллисекунд. Аналог check_mouse_activity() из Ncurses.
{
    if (rawBegin < rawEnd)
        return true;
    if (fd < 0)
        return false;
    // Используем select вместо poll, поскольку poll не поддерживает
    // терминальные устройства macOS.
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
// Перенос kgetch() из Ncurses: читает последовательность клавиши и
// возвращает её код либо первый байт, если последовательность не распознана.
{
    int node = keyTrie.root();
    for (;;)
    {
        if (fifoPeek > 0 && fifoAt(0) >= KEY_MIN)
            break;
        else if (fifoPeek >= fifoCount)
        {
            // Необработанных элементов нет: читаем ещё.
            int k = rawGet(0);
            if (k == -1 || !fifoPushBack(k))
            {
                fifoPeek = 0; // Элементы остаются неинтерпретированными.
                return -1;
            }
        }

        int ch = fifoAt(fifoPeek++);
        if (ch >= KEY_MIN)
        {
            // Код клавиши, возвращённый в середину очереди.
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
            // Последовательность завершена: удаляем её из очереди.
            int value = keyTrie.value(node);
            size_t n = fifoPeek;
            fifoPeek = 0;
            while (n-- > 0)
                fifoPopFront();
            return value;
        }
        node = keyTrie.child(node);

        if (fifoPeek >= fifoCount && !hasActivity(escDelayMs))
            break; // Время ожидания истекло.
    }
    fifoPeek = 0;
    int ch = fifoPopFront();
    fifoPeek = 0;
    return ch;
}

int TerminalInputGetter::get(int ms) noexcept
// Перенос wgetch() из Ncurses в режиме клавиатуры.
{
    if (fifoPeek > 0)
        return fifoPopFront();
    if (fifoCount == 0 && !hasActivity(ms))
        return -1;

    // Как Ncurses, накапливаем непрерывно поступающие события мыши.
    // Поскольку драйвер мыши Ncurses не используется, после kmous просто
    // ждём дополнительный ввод и возвращаем следующий элемент в очередь для
    // разбора события.
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
// Перенос ungetch() из Ncurses.
{
    if (k == -1 || fifoCount >= fifoSize)
        return;
    bool wasEmpty = fifoCount == 0;
    fifoPushFront(k);
    if (wasEmpty)
        fifoPeek = fifoCount; // Необработанных элементов нет.
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
    // Как _nc_init_keytry() в Ncurses: сначала добавляем стандартные
    // возможности в порядке standardKeys; более поздняя заменяет прежнюю с
    // той же последовательностью. Затем добавляем пользовательские
    // возможности с именами на k, если последовательность ещё не определена.
    //
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
    // Примечание: из-за ошибки на единицу Turbo Vision раньше игнорировала
    // первую пользовательскую клавишу с кодом KEY_MAX.
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
    // Не задерживаем слишком надолго нажатие Esc: в современных терминалах
    // Alt обычно работает хорошо. Задержка всё же помогает различать
    // специальные последовательности. Чтение практически неблокирующее;
    // некоторые терминалы подают данные медленно. Риск блокировки есть
    // только при чтении многобайтового символа или управляющей
    // последовательности.
    in(aCon.in(), keyTrie, getEnv<int>("ESCDELAY", 10), readTimeoutMs)
{
    buildKeyTable(display.termInfo, keyTrie, extKeyNames);
    // Перехватываем весь ввод клавиатуры без эха и преобразования CR.
    display.setRawInputMode();
    // Разрешаем перехват функциональных клавиш.
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
        // Символ Unicode может занимать до четырёх байтов UTF-8.
        int keys[4] = {k}, num_keys = 1;

        ev.what = evKeyDown;
        bool Alt = false;

        if (keys[0] == KEY_ESC)
            detectAlt(keys, Alt);

        if ((uint) keys[0] < 32)
            ev.keyDown = fromNonPrintableAscii[keys[0]];
        else if (keys[0] == 127)
            ev.keyDown = {{kbBack}, 0}; // ^?, Delete.
        else if (KEY_MIN < keys[0])
            ev.keyDown = keyFromCode(keys[0], extKeyNames);

        // Если предыдущие таблицы не изменили значение и это не код клавиши,
        // считаем его печатным символом.
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
/*
 * Сочетания Alt+клавиша начинаются с Esc. Проверяем наличие следующего символа: если он уже получен, считаем
 * это Alt+клавиша. Другие данные терминала тоже могут начинаться с Esc, но большинство уже распознано.
 */
    int k = getChNb();
    if (k != -1)
    {
        keys[0] = k;
        Alt = true;
    }
}

void TerminalInput::parsePrintableChar(TEvent &ev, int keys[4], int &num_keys) noexcept
{
    // Читаем возможные оставшиеся байты.
    readUtf8Char(keys, num_keys);
    for (int i = 0; i < num_keys; ++i)
        ev.keyDown.text[i] = (char) keys[i];
    ev.keyDown.textLength = (uchar) num_keys;
    // Возможно, символ представим в активной кодовой странице.
    //
    ev.keyDown.charScan.charCode = CpTranslator::fromUtf8({ev.keyDown.text, size_t(num_keys)});
    // Не допускаем срабатывания сочетаний Ctrl+клавиша от текстового ввода.
    if (ev.keyDown.keyCode <= kbCtrlZ)
        ev.keyDown.keyCode = kbNoKey;
}

void TerminalInput::readUtf8Char(int keys[4], int &num_keys) noexcept
{
/* Терминал передаёт Unicode побайтно. Нужно определить число байтов символа и прочитать их все.
 */
    num_keys += Utf8BytesLeft((char) keys[0]);
    for (int i = 1; i < num_keys; ++i)
        if ((keys[i] = in.get()) == -1)
        {
            num_keys = i;
            break;
        }
}

bool TerminalInput::parseMouse(TEvent &ev) noexcept
// Предусловие: только что прочитана последовательность возможности kmous.
{
    // kmous имеет вид "\x1B[M" либо "\x1B[<", но неизвестно, какой вариант
    // прочитан. Можно запросить возможности терминала либо попробовать оба
    // разбора. parseSGRMouse с большей вероятностью обнаружит
    // несоответствие, поэтому начинаем с него.
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

} // пространство имён tvision

#endif // _TV_UNIX
