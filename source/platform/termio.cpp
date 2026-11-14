#define Uses_TKeys
#include <tvision/tv.h>

#include <internal/termio.h>
#include <internal/far2l.h>
#include <internal/conctl.h>
#include <internal/constmap.h>
#include <internal/constarr.h>
#include <internal/codepage.h>
#include <internal/win32con.h>
#include <internal/getenv.h>
#include <internal/base64.h>
#include <internal/utf8.h>

#include <chrono>

namespace tvision
{

static const const_unordered_map<ushort, constarray<ushort, 3>> moddedKeyCodes =
{
    { 'A', {0, kbCtrlA, kbAltA}                }, { 'B', {0, kbCtrlB, kbAltB}                },
    { 'C', {0, kbCtrlC, kbAltC}                }, { 'D', {0, kbCtrlD, kbAltD}                },
    { 'E', {0, kbCtrlE, kbAltE}                }, { 'F', {0, kbCtrlF, kbAltF}                },
    { 'G', {0, kbCtrlG, kbAltG}                }, { 'H', {0, kbCtrlH, kbAltH}                },
    { 'I', {0, kbCtrlI, kbAltI}                }, { 'J', {0, kbCtrlJ, kbAltJ}                },
    { 'K', {0, kbCtrlK, kbAltK}                }, { 'L', {0, kbCtrlL, kbAltL}                },
    { 'M', {0, kbCtrlM, kbAltM}                }, { 'N', {0, kbCtrlN, kbAltN}                },
    { 'O', {0, kbCtrlO, kbAltO}                }, { 'P', {0, kbCtrlP, kbAltP}                },
    { 'Q', {0, kbCtrlQ, kbAltQ}                }, { 'R', {0, kbCtrlR, kbAltR}                },
    { 'S', {0, kbCtrlS, kbAltS}                }, { 'T', {0, kbCtrlT, kbAltT}                },
    { 'U', {0, kbCtrlU, kbAltU}                }, { 'V', {0, kbCtrlV, kbAltV}                },
    { 'W', {0, kbCtrlW, kbAltW}                }, { 'X', {0, kbCtrlX, kbAltX}                },
    { 'Y', {0, kbCtrlY, kbAltY}                }, { 'Z', {0, kbCtrlZ, kbAltZ}                },
    { '1', {0, 0, kbAlt1}                      }, { '2', {0, 0, kbAlt2}                      },
    { '3', {0, 0, kbAlt3}                      }, { '4', {0, 0, kbAlt4}                      },
    { '5', {0, 0, kbAlt5}                      }, { '6', {0, 0, kbAlt6}                      },
    { '7', {0, 0, kbAlt7}                      }, { '8', {0, 0, kbAlt8}                      },
    { '9', {0, 0, kbAlt9}                      }, { '0', {0, 0, kbAlt0}                      },
    { ' ', {0, 0, kbAltSpace}                  }, { '-', {0, 0, kbAltMinus}                  },
    { '=', {0, 0, kbAltEqual}                  },
    { kbF1, {kbShiftF1, kbCtrlF1, kbAltF1}     }, { kbF2, {kbShiftF2, kbCtrlF2, kbAltF2}     },
    { kbF3, {kbShiftF3, kbCtrlF3, kbAltF3}     }, { kbF4, {kbShiftF4, kbCtrlF4, kbAltF4}     },
    { kbF5, {kbShiftF5, kbCtrlF5, kbAltF5}     }, { kbF6, {kbShiftF6, kbCtrlF6, kbAltF6}     },
    { kbF7, {kbShiftF7, kbCtrlF7, kbAltF7}     }, { kbF8, {kbShiftF8, kbCtrlF8, kbAltF8}     },
    { kbF9, {kbShiftF9, kbCtrlF9, kbAltF9}     }, { kbF10, {kbShiftF10, kbCtrlF10, kbAltF10} },
    { kbF11, {kbShiftF11, kbCtrlF11, kbAltF11} }, { kbF12, {kbShiftF12, kbCtrlF12, kbAltF12} },
    { kbEsc, {0, 0, kbAltEsc}                  }, { kbBack, {0, kbCtrlBack, kbAltBack}       },
    { kbTab, {kbShiftTab, kbCtrlTab, kbAltTab} }, { kbEnter, {0, kbCtrlEnter, kbAltEnter}    },
    { kbHome, {0, kbCtrlHome, kbAltHome}       }, { kbUp, {0, kbCtrlUp, kbAltUp}             },
    { kbPgUp, {0, kbCtrlPgUp, kbAltPgUp}       }, { kbLeft, {0, kbCtrlLeft, kbAltLeft}       },
    { kbRight, {0, kbCtrlRight, kbAltRight}    }, { kbEnd, {0, kbCtrlEnd, kbAltEnd}          },
    { kbDown, {0, kbCtrlDown, kbAltDown}       }, { kbPgDn, {0, kbCtrlPgDn, kbAltPgDn}       },
    { kbIns, {kbShiftIns, kbCtrlIns, kbAltIns} }, { kbDel, {kbShiftDel, kbCtrlDel, kbAltDel} },
};

static ushort getModdedKeyCode(ushort keyCode, ushort tvMods)
{
    // Приоритет модификаторов: Shift < Ctrl < Alt.
    int largestMod = (tvMods & kbLeftAlt) ? 2
                   : (tvMods & kbLeftCtrl) ? 1
                   : 0;
    return moddedKeyCodes[keyCode][largestMod];
}

const uint XTermModDefault = 1;

static KeyDownEvent keyWithXTermMods(ushort keyCode, uint mods) noexcept
{
    mods -= XTermModDefault;
    ushort tvMods =
          (kbShift & -(mods & 1))
        | (kbLeftAlt & -(mods & 2))
        | (kbLeftCtrl & -(mods & 4))
        ;
    KeyDownEvent keyDown {{keyCode}, tvMods};
    TermIO::normalizeKey(keyDown);
    return keyDown;
}

static bool isAlpha(uint32_t ascii) noexcept
{
    return ' ' <= ascii && ascii < 127;
};

static bool isAsciiLetter(uint32_t ch) noexcept
{
    return ('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z');
};

static bool isPrivate(uint32_t codepoint) noexcept
{
    return 57344 <= codepoint && codepoint <= 63743;
};

static bool keyFromCodepoint(uint value, uint mods, KeyDownEvent &keyDown) noexcept
{
    ushort keyCode = 0;
    switch (value)
    {
        case     8: keyCode = kbBack;   break;
        case     9: keyCode = kbTab;    break;
        case    13: keyCode = kbEnter;  break;
        case    27: keyCode = kbEsc;    break;
        case   127: keyCode = kbBack;   break;
        // Функциональные клавиши протокола Kitty:
        // https://sw.kovidgoyal.net/kitty/keyboard-protocol.html#functional.
        // Цифровая клавиатура.
        case 57399: keyCode = '0';      break;
        case 57400: keyCode = '1';      break;
        case 57401: keyCode = '2';      break;
        case 57402: keyCode = '3';      break;
        case 57403: keyCode = '4';      break;
        case 57404: keyCode = '5';      break;
        case 57405: keyCode = '6';      break;
        case 57406: keyCode = '7';      break;
        case 57407: keyCode = '8';      break;
        case 57408: keyCode = '9';      break;
        case 57409: keyCode = '.';      break;
        case 57410: keyCode = '/';      break;
        case 57411: keyCode = '*';      break;
        case 57412: keyCode = '-';      break;
        case 57413: keyCode = '+';      break;
        case 57414: keyCode = kbEnter;  break;
        case 57415: keyCode = '=';      break;
        case 57416: keyCode = ',';      break;
        case 57417: keyCode = kbLeft;   break;
        case 57418: keyCode = kbRight;  break;
        case 57419: keyCode = kbUp;     break;
        case 57420: keyCode = kbDown;   break;
        case 57421: keyCode = kbPgUp;   break;
        case 57422: keyCode = kbPgDn;   break;
        case 57423: keyCode = kbHome;   break;
        case 57424: keyCode = kbEnd;    break;
        case 57425: keyCode = kbIns;    break;
        case 57426: keyCode = kbDel;    break;
        default: if (isAlpha(value)) keyCode = value;
    }
    keyDown = keyWithXTermMods(keyCode, mods);
    if ( isAlpha(keyDown.keyCode) ||
         (keyDown.keyCode == 0 && ' ' <= value && !isPrivate(value)) )
    {
        uint32_t codepoint = keyDown.keyCode == 0 ? value : keyDown.keyCode;
        keyDown.textLength = utf32To8(codepoint, keyDown.text);
        keyDown.charScan.charCode =
            CpTranslator::printableFromUtf8({keyDown.text, keyDown.textLength});
    }
    return keyDown.keyCode != 0 || keyDown.textLength != 0;
}

static bool keyFromLetter(uint letter, uint mod, KeyDownEvent &keyDown) noexcept
{
    ushort keyCode = 0;
    switch (letter)
    {
        case 'A': keyCode = kbUp; break;
        case 'B': keyCode = kbDown; break;
        case 'C': keyCode = kbRight; break;
        case 'D': keyCode = kbLeft; break;
        case 'E': keyCode = kbNoKey; break; // Клавиша 5 цифровой клавиатуры, KP_Begin.
        case 'F': keyCode = kbEnd; break;
        case 'H': keyCode = kbHome; break;
        case 'P': keyCode = kbF1; break;
        case 'Q': keyCode = kbF2; break;
        case 'R': keyCode = kbF3; break;
        case 'S': keyCode = kbF4; break;
        case 'Z': keyCode = kbTab; break;
        // Цифровая клавиатура XTerm, SS3.
        case 'j': keyCode = '*'; break;
        case 'k': keyCode = '+'; break;
        case 'm': keyCode = '-'; break;
        case 'M': keyCode = kbEnter; break;
        case 'n': keyCode = kbDel; break;
        case 'o': keyCode = '/'; break;
        case 'p': keyCode = kbIns; break;
        case 'q': keyCode = kbEnd; break;
        case 'r': keyCode = kbDown; break;
        case 's': keyCode = kbPgDn; break;
        case 't': keyCode = kbLeft; break;
        case 'u': keyCode = kbNoKey; break; // Клавиша 5 цифровой клавиатуры, KP_Begin.
        case 'v': keyCode = kbRight; break;
        case 'w': keyCode = kbHome; break;
        case 'x': keyCode = kbUp; break;
        case 'y': keyCode = kbPgUp; break;
        default: return false;
    }
    keyDown = keyWithXTermMods(keyCode, mod);
    if (isAlpha(keyDown.keyCode))
    {
        keyDown.text[0] = keyDown.keyCode;
        keyDown.textLength = 1;
    }
    return true;
}

void GetChBuf::reject() noexcept
{
    while (size)
        unget();
}

// getNum, getInt: инвариант — последний прочитанный нецифровой код либо
// -1 доступен через last() и может быть возвращён в очередь.

bool GetChBuf::getNum(uint &result) noexcept
{
    uint num = 0, digits = 0;
    int k;
    while ((k = get(true)) != -1 && '0' <= k && k <= '9')
    {
        num = 10 * num + (k - '0');
        ++digits;
    }
    if (digits)
        return (result = num), true;
    return false;
}

bool GetChBuf::getInt(int &result) noexcept
{
    int num = 0, digits = 0, sign = 1;
    int k = get(true);
    if (k == '-')
    {
        sign = -1;
        k = get(true);
    }
    while (k != -1 && '0' <= k && k <= '9')
    {
        num = 10 * num + (k - '0');
        ++digits;
        k = get(true);
    }
    if (digits)
        return (result = sign*num), true;
    return false;
}

bool GetChBuf::readStr(TStringView str) noexcept
{
    size_t origSize = size;
    size_t i = 0;
    while (i < str.size() && get() == str[i])
        ++i;
    if (i == str.size())
        return true;
    while (origSize < size)
        unget();
    return false;
}

bool CSIData::readFrom(GetChBuf &buf) noexcept
// Предусловие: только что прочитано "\x1B[".
{
    for (uint i = 0; i < maxLength; ++i)
    {
        if (!buf.getNum(_values[i]))
            _values[i] = UINT_MAX;
        int k = buf.last();
        if (k == -1)
            // Ввод закончился, но CSI ещё не завершена.
            return false;
        if (k == ';' || k == ':')
            _separators[i] = (char) k;
        else
        {
            terminator = (char) k;
            length = i + 1;
            return true;
        }
    }
    // CSI может превышать поддерживаемую длину.
    return false;
}

// События мыши запрашиваются и разбираются вручную, без зависимости от
// терминальной библиотеки.

void TermIO::mouseOn(ConsoleCtl &con) noexcept
{
    TStringView seq = "\x1B[?1001s" // Сохраняем прежний режим передачи выделения мышью.
                      "\x1B[?1000h" // Включаем передачу событий мыши.
                      "\x1B[?1002h" // Включаем передачу перетаскиваний мышью.
                      "\x1B[?1006h" // Включаем расширенные события мыши SGR.
                    ;
    con.write(seq.data(), seq.size());
}

void TermIO::mouseOff(ConsoleCtl &con) noexcept
{
    TStringView seq = "\x1B[?1006l" // Отключаем расширенные события мыши SGR.
                      "\x1B[?1002l" // Отключаем передачу перетаскиваний мышью.
                      "\x1B[?1000l" // Отключаем передачу событий мыши.
                      "\x1B[?1001r" // Восстанавливаем прежний режим передачи выделения мышью.
                    ;
    con.write(seq.data(), seq.size());
}

void TermIO::keyModsOn(ConsoleCtl &con) noexcept
{
    char buf[256];

    strcpy(buf,
        "\x1B[?1036s"   // Сохраняем metaSendsEscape в XTerm.
        "\x1B[?1036h"   // Включаем metaSendsEscape в XTerm.
        "\x1B[?2004s"   // Сохраняем режим обрамлённой вставки.
        "\x1B[?2004h"   // Включаем режим обрамлённой вставки.
        "\x1B[>4;1m"    // Включаем modifyOtherKeys в XTerm.
        "\x1B[>5u"      // Устраняем неоднозначность управляющих кодов, флаг 1, и включаем альтернативные коды клавиш, флаг 4, в Kitty.
        "\x1B[?9001h"   // Включаем win32-input-mode в Conpty.
        far2lEnableSeq  // Включаем терминальные расширения far2l.
    );

    if (char *term = getenv("TERM"))
    {
        // Проверяем полную поддержку буфера обмена OSC 52.
        if (strstr(term, "alacritty") || strstr(term, "foot"))
            strcat(buf,
                // Запрашиваем содержимое буфера обмена, чтобы проверить возможность
                // чтения. Нельзя безусловно выводить этот запрос: используем только
                // известные поддерживающие значения TERM.
                "\x1B]52;;?\x07"
            );
        else
            strcat(buf,
                // Проверяем возможность kitty-query-clipboard_control через XTGETTCAP.
                "\x1BP+q6b697474792d71756572792d636c6970626f6172645f636f6e74726f6c\x1B\\"
                // Проверяем allowWindowOps через XTQALLOWED.
                "\x1B]60\x1B\\"
            );
    }

    strcat(buf,
        // Некоторые терминалы не распознают предыдущие последовательности и
        // выводят их как текст. Поэтому очищаем экран.
        "\x1B[2J"
    );

    con.write(buf, strlen(buf));
}

void TermIO::keyModsOff(ConsoleCtl &con) noexcept
{
    TStringView seq = far2lDisableSeq
                      "\x1B[?9001l" // Отключаем win32-input-mode в Conpty.
                      "\x1B[<u"     // Восстанавливаем прежний режим клавиатуры Kitty.
                      "\x1B[>4m"    // Сбрасываем modifyOtherKeys в XTerm.
                      "\x1B[?2004l" // Отключаем обрамлённую вставку.
                      "\x1B[?2004r" // Восстанавливаем обрамлённую вставку.
                      "\x1B[?1036r" // Восстанавливаем metaSendsEscape в XTerm.
                    ;
    con.write(seq.data(), seq.size());
}

void TermIO::normalizeKey(KeyDownEvent &keyDown) noexcept
{
    TKey tKey(keyDown);
    ushort newMods = tKey.mods & (kbShift | kbLeftCtrl | kbLeftAlt);
    if (newMods != 0)
        if (ushort keyCode = getModdedKeyCode(tKey.code, newMods))
        {
            keyDown.keyCode = keyCode;
            if (keyDown.charScan.charCode < ' ')
                keyDown.textLength = 0;
        }
    // TKey не различает левые и правые модификаторы, поэтому сохраняем их
    // при наличии.
    ushort origMods = keyDown.controlKeyState;
    keyDown.controlKeyState =
        ((origMods | newMods) & ~(kbCtrlShift | kbAltShift))
      | ((origMods & kbCtrlShift ? origMods : newMods) & kbCtrlShift)
      | ((origMods & kbAltShift ? origMods : newMods) & kbAltShift)
        ;
}

ParseResult TermIO::parseEvent(GetChBuf &buf, TEvent &ev, InputState &state) noexcept
{
    if (buf.get() == '\x1B')
        return parseEscapeSeq(buf, ev, state);
    return Rejected;
}

ParseResult TermIO::parseEscapeSeq(GetChBuf &buf, TEvent &ev, InputState &state) noexcept
// Предусловие: только что прочитано "\x1B".
{
    ParseResult res = Rejected;
    switch (buf.get())
    {
        case '_':
            if (buf.readStr("f2l"))
                return parseFar2lInput(buf, ev, state);
            if (buf.readStr("far2l"))
                return parseFar2lAnswer(buf, ev, state);
            break;
        case '[':
            switch (buf.get())
            {
                // События мыши в кодировках X10 и SGR.
                case 'M':
                    return parseX10Mouse(buf, ev, state) == Accepted ? Accepted : Ignored;
                case '<':
                    return parseSGRMouse(buf, ev, state) == Accepted ? Accepted : Ignored;
                default:
                {
                    buf.unget();
                    CSIData csi;
                    if (csi.readFrom(buf))
                    {
                        switch (csi.terminator)
                        {
                            case 'u':
                                return parseKittyKey(csi, ev);
                            case 'R':
                                return parseCPR(csi, state);
                            case '_':
                                return parseWin32InputModeKeyOrEscapeSeq(csi, buf.in, ev, state);
                            default:
                                return parseCSIKey(csi, ev, state);
                        }
                    }
                    break;
                }
            }
            break;
        case 'O':
            return parseSS3Key(buf, ev);
        case 'P':
            return parseDCS(buf, state);
        case ']':
            return parseOSC(buf, state);
        case '\x1B':
            res = parseEscapeSeq(buf, ev, state);
            if (res == Accepted && ev.what == evKeyDown)
            {
                ev.keyDown.controlKeyState |= kbLeftAlt;
                normalizeKey(ev.keyDown);
            }
            break;
    }
    return res;
}

const ushort
    mmAlt = 0x08,
    mmCtrl = 0x10;

ParseResult TermIO::parseX10Mouse(GetChBuf &buf, TEvent &ev, InputState &state) noexcept
// Предусловие: прочитано "\x1B[M". Полная последовательность имеет вид
// "\x1B[Mabc": a — номер кнопки плюс 32; b — номер столбца от единицы
// плюс 32; c — номер строки от единицы плюс 32.
//
//
{
    uint butm = (uint) buf.get();
    uint mod = butm & (mmAlt | mmCtrl);
    uint but = (butm & ~(mmAlt | mmCtrl)) - 32;
    if (255 - 32 < but) return Rejected;
    int col, row;
    for (int *i : {&col, &row})
    {
        *i = buf.get();
        if (*i < 0 || 255 < *i)
            return Rejected;
        // Теоретически эта кодировка ограничена координатами [0, 222]. Но
        // некоторые терминалы, например urxvt, продолжают увеличивать счётчики
        // до переполнения. Это позволяет поддержать больше координат. Отклонять
        // такую последовательность нельзя: иначе появятся события Ctrl+клавиша.
        //
        if (*i > 32)
            *i -= 32;
        else
            *i += (256 - 32);
        // Переводим к нумерации с нуля.
        --*i;
    }

    ev.what = evMouse;
    ev.mouse = {};
    ev.mouse.where = {col, row};
    ev.mouse.controlKeyState = (-!!(mod & mmAlt) & kbLeftAlt) | (-!!(mod & mmCtrl) & kbLeftCtrl);
    switch (but)
    {
        case 0: // Press.
        case 32: // Drag.
            state.buttons |= mbLeftButton; break;
        case 1:
        case 33:
            state.buttons |= mbMiddleButton; break;
        case 2:
        case 34:
            state.buttons |= mbRightButton; break;
        case 3: state.buttons = 0; break; // Release.
        case 64: ev.mouse.wheel = mwUp; break;
        case 65: ev.mouse.wheel = mwDown; break;
        case 66: ev.mouse.wheel = mwLeft; break;
        case 67: ev.mouse.wheel = mwRight; break;
    }
    ev.mouse.buttons = state.buttons;
    return Accepted;
}

ParseResult TermIO::parseSGRMouse(GetChBuf &buf, TEvent &ev, InputState &state) noexcept
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Extended-coordinates.
// Предусловие: прочитано "\x1B[<". Полный вид — "\x1B[<a;b;cM" либо
// "\x1B[<a;b;cm": a — десятичный номер кнопки; b и c — десятичные номера
// столбца и строки от единицы. M завершает нажатие, m — отпускание.
//
//
//
{
    uint butm;
    if (!buf.getNum(butm)) return Rejected;
    uint mod = butm & (mmAlt | mmCtrl);
    uint but = butm & ~(mmAlt | mmCtrl);
    // IntelliJ может передавать отрицательные координаты.
    int col, row;
    if (!buf.getInt(col) || !buf.getInt(row)) return Rejected;
    // Переводим координаты к нумерации с нуля.
    row = max(row, 1);
    col = max(col, 1);
    --row, --col;
    // В конце — состояние нажатия или отпускания.
    uint type = (uint) buf.last();
    if (!(type == 'M' || type == 'm')) return Rejected;

    ev.what = evMouse;
    ev.mouse = {};
    ev.mouse.where = {col, row};
    ev.mouse.controlKeyState = (-!!(mod & mmAlt) & kbLeftAlt) | (-!!(mod & mmCtrl) & kbLeftCtrl);
    if (type == 'M') // Нажатие, колесо или перетаскивание.
    {
        switch (but)
        {
            case 0:
            case 32:
                state.buttons |= mbLeftButton; break;
            case 1:
            case 33:
                state.buttons |= mbMiddleButton; break;
            case 2:
            case 34:
                state.buttons |= mbRightButton; break;
            case 64: ev.mouse.wheel = mwUp; break;
            case 65: ev.mouse.wheel = mwDown; break;
            case 66: ev.mouse.wheel = mwLeft; break;
            case 67: ev.mouse.wheel = mwRight; break;
        }
    }
    else // Release.
    {
        switch (but)
        {
            case 0: state.buttons &= ~mbLeftButton; break;
            case 1: state.buttons &= ~mbMiddleButton; break;
            case 2: state.buttons &= ~mbRightButton; break;
        }
    }
    ev.mouse.buttons = state.buttons;
    return Accepted;
}

// Функции ниже разбирают отдельные последовательности терминалов, не
// соответствующие их описаниям terminfo/termcap: например Shift+F1–F4 в
// Konsole и F1–F4 в Putty. Это проще исправления приложения или
// обновления базы терминалов.

ParseResult TermIO::parseCSIKey(const CSIData &csi, TEvent &ev, InputState &state) noexcept
// https://invisible-island.net/xterm/xterm-function-keys.html
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html
{
    uint terminator = csi.terminator;
    if (csi.length == 1 && terminator == '~')
    {
        switch (csi.getValue(0))
        {
            case 1: ev.keyDown = {{kbHome}}; break;
            case 2: ev.keyDown = {{kbIns}}; break;
            case 3: ev.keyDown = {{kbDel}}; break;
            case 4: ev.keyDown = {{kbEnd}}; break;
            case 5: ev.keyDown = {{kbPgUp}}; break;
            case 6: ev.keyDown = {{kbPgDn}}; break;
            // Эти числа допускают разные трактовки, например F1–F12 вместо F1–F10.
            // Но запасной разбор нужен Putty, который использует F1–F10.
            //
            case 11: ev.keyDown = {{kbF1}}; break;
            case 12: ev.keyDown = {{kbF2}}; break;
            case 13: ev.keyDown = {{kbF3}}; break;
            case 14: ev.keyDown = {{kbF4}}; break;
            case 15: ev.keyDown = {{kbF5}}; break;
            case 17: ev.keyDown = {{kbF6}}; break;
            case 18: ev.keyDown = {{kbF7}}; break;
            case 19: ev.keyDown = {{kbF8}}; break;
            case 20: ev.keyDown = {{kbF9}}; break;
            case 21: ev.keyDown = {{kbF10}}; break;
            case 23: ev.keyDown = {{kbShiftF1}, kbShift}; break;
            case 24: ev.keyDown = {{kbShiftF2}, kbShift}; break;
            case 25: ev.keyDown = {{kbShiftF3}, kbShift}; break;
            case 26: ev.keyDown = {{kbShiftF4}, kbShift}; break;
            case 28: ev.keyDown = {{kbShiftF5}, kbShift}; break;
            case 29: ev.keyDown = {{kbShiftF6}, kbShift}; break;
            case 31: ev.keyDown = {{kbShiftF7}, kbShift}; break;
            case 32: ev.keyDown = {{kbShiftF8}, kbShift}; break;
            case 33: ev.keyDown = {{kbShiftF9}, kbShift}; break;
            case 34: ev.keyDown = {{kbShiftF10}, kbShift}; break;
            case 200: state.bracketedPaste = true; return Ignored;
            case 201: state.bracketedPaste = false; return Ignored;
            default: return Rejected;
        }
    }
    else if (csi.length == 1 && csi.getValue(0) == 1)
    {
        if (!keyFromLetter(terminator, XTermModDefault, ev.keyDown))
            return Rejected;
    }
    else if (csi.length == 2 && csi.getSeparator(0) == ';')
    {
        uint mod = csi.getValue(1);
        if (csi.getValue(0) == 1)
        {
            if (!keyFromLetter(terminator, mod, ev.keyDown))
                return Rejected;
        }
        else if (terminator == '~')
        {
            ushort keyCode = 0;
            switch (csi.getValue(0))
            {
                case  2: keyCode = kbIns; break;
                case  3: keyCode = kbDel; break;
                case  5: keyCode = kbPgUp; break;
                case  6: keyCode = kbPgDn; break;
                case 11: keyCode = kbF1; break;
                case 12: keyCode = kbF2; break;
                case 13: keyCode = kbF3; break;
                case 14: keyCode = kbF4; break;
                case 15: keyCode = kbF5; break;
                case 17: keyCode = kbF6; break;
                case 18: keyCode = kbF7; break;
                case 19: keyCode = kbF8; break;
                case 20: keyCode = kbF9; break;
                case 21: keyCode = kbF10; break;
                case 23: keyCode = kbF11; break;
                case 24: keyCode = kbF12; break;
                case 29: keyCode = kbNoKey; break; // Клавиша меню XTerm.
                default: return Rejected;
            }
            ev.keyDown = keyWithXTermMods(keyCode, csi.getValue(1));
        }
        else
            return Rejected;
    }
    else if ( csi.length == 3 &&
              csi.getValue(0) == 27 &&
              csi.getSeparator(0) == ';' &&
              csi.getSeparator(1) == ';' &&
              terminator == '~' )
    {
        // Режим modifyOtherKeys в XTerm.
        uint key = csi.getValue(2);
        uint mod = csi.getValue(1);
        if (!keyFromCodepoint(key, mod, ev.keyDown))
            return Ignored;
    }
    else
        return Rejected;
    ev.what = evKeyDown;
    return Accepted;
}

ParseResult TermIO::parseSS3Key(GetChBuf &buf, TEvent &ev) noexcept
// https://invisible-island.net/xterm/xterm-function-keys.html.
// Предусловие: прочитано "\x1BO". Konsole, IntelliJ.
//
{
    uint mod;
    if (!buf.getNum(mod)) return Rejected;
    uint key = (uint) buf.last();
    if (!keyFromLetter(key, mod, ev.keyDown)) return Rejected;
    ev.what = evKeyDown;
    return Accepted;
}

ParseResult TermIO::parseKittyKey(const CSIData &csi, TEvent &ev) noexcept
// https://sw.kovidgoyal.net/kitty/keyboard-protocol.html
// http://www.leonerd.org.uk/hacks/fixterms/
{
    if (csi.length < 1 || csi.terminator != 'u')
        return Rejected;

    // ПРИМЕЧАНИЕ: keyModsOn() запрашивает не все возможности протокола
    // Kitty, однако код их поддерживает. Событие имеет вид: unicode-key-code
    // : shifted-key-code : base-layout-key ; modifiers : event-type ;
    // text-as-codepoints u. Обязателен только unicode-key-code, остальные
    // части необязательны.
    //
    //
    //

    uint kittyKeyCode = 0;
    uint kittyShiftedKeyCode = 0;
    uint kittyBaseLayoutKey = 0;
    uint kittyModifiers = 1;
    uint kittyEventType = 1;
    uint kittyText = 0;

    uint i = 0;
    kittyKeyCode = csi.getValue(i++, 0);
    if (i < csi.length && csi.getSeparator(i - 1) == ':')
        kittyShiftedKeyCode = csi.getValue(i++, 0);
    if (i < csi.length && csi.getSeparator(i - 1) == ':')
        kittyBaseLayoutKey = csi.getValue(i++, 0);
    if (i < csi.length)
        kittyModifiers = csi.getValue(i++, 1);
    if (i < csi.length && csi.getSeparator(i - 1) == ':')
        kittyEventType = csi.getValue(i++, 1);
    if (i < csi.length)
        // Теоретически символов может быть несколько, но сейчас поддерживается
        // только первый.
        kittyText = csi.getValue(i++, 0);

    if (kittyEventType != 1)
        // Нас интересуют только события нажатия.
        return Ignored;

    uint codepoint = kittyKeyCode;
    if (kittyText != 0)
        codepoint = kittyText;
    else if (kittyShiftedKeyCode != 0)
        codepoint = kittyShiftedKeyCode;

    if (!keyFromCodepoint(codepoint, kittyModifiers, ev.keyDown))
        return Ignored;

    // Если unicode-key-code не является буквой ASCII, но base-layout-key
    // является ею и нажаты Ctrl или Alt, задаём keyCode как Ctrl/Alt+A–Z.
    // Так стандартные сочетания работают и в не-ASCII-раскладках: например
    // Ctrl+Ф в русской раскладке соответствует Ctrl+A.
    //
    if ( !isAsciiLetter(kittyKeyCode) && isAsciiLetter(kittyBaseLayoutKey) &&
         (ev.keyDown.controlKeyState & (kbCtrlShift | kbAltShift)) != 0 )
    {
        // base-layout-key в Kitty всегда задаётся в нижнем регистре.
        char upperBaseLayoutKey = kittyBaseLayoutKey - 'a' + 'A';
        ev.keyDown.keyCode = getModdedKeyCode(upperBaseLayoutKey, ev.keyDown.controlKeyState);
        // ev.keyDown.text всё равно содержит исходный текст клавиши.
    }

    ev.what = evKeyDown;
    return Accepted;
}

ParseResult TermIO::parseDCS(GetChBuf &buf, InputState &state) noexcept
// Предусловие: только что прочитано '\x1BP'.
{
    if (char *s = readUntilBelOrSt(buf))
    {
        // Получаем DCS только в ответ на наш запрос возможностей Kitty.
        if (strstr(s, "726561642d636c6970626f617264")) // 'read-clipboard'
            state.hasFullOsc52 = true;
        free(s);
    }
    return Ignored;
}

ParseResult TermIO::parseOSC(GetChBuf &buf, InputState &state) noexcept
// Предусловие: только что прочитано '\x1B]'.
{
    if (char *s = readUntilBelOrSt(buf))
    {
        TStringView sv(s);
        if (sv.size() > 3 && sv.substr(0, 3) == "52;") // OSC 52
        {
            if (char *begin = (char *) memchr(&sv[3], ';', sv.size() - 3))
            {
                if (!state.hasFullOsc52)
                    // Получен ответ на первоначальный запрос.
                    state.hasFullOsc52 = true;
                else if (state.putPaste)
                {
                    TStringView encoded = sv.substr(begin + 1 - &sv[0]);
                    if (char *pDecoded = (char *) malloc((encoded.size() * 3)/4 + 3))
                    {
                        TStringView decoded = decodeBase64(encoded, pDecoded);
                        state.putPaste(decoded);
                        free(pDecoded);
                    }
                }
            }
        }
        else if (sv.size() > 3 && sv.substr(0, 3) == "60;") // OSC 60
            if (strstr(&sv[3], "allowWindowOps"))
                state.hasFullOsc52 = true;
        free(s);
    }
    return Ignored;
}

ParseResult TermIO::parseCPR(const CSIData &csi, InputState &state) noexcept
// Предусловие: csi.terminator == 'R'. Отчёт о позиции курсора приходит в
// ответ на запрос состояния устройства из consumeUnprocessedInput().
//
{
    if (csi.length != 2 || csi.getSeparator(0) != ';')
        return Rejected;

    state.gotDsrResponse = true;
    return Ignored;
}

static ParseResult parseWin32InputModeKey(const CSIData &csi, TEvent &ev, InputState &state) noexcept
// https://github.com/microsoft/terminal/blob/main/doc/specs/%234999%20-%20Improved%20keyboard%20handling%20in%20Conpty.md
{
    KEY_EVENT_RECORD kev;
    kev.wVirtualKeyCode = (ushort) csi.getValue(0, 0);
    kev.wVirtualScanCode = (ushort) csi.getValue(1, 0);
    kev.uChar.UnicodeChar = (ushort) csi.getValue(2, 0);
    kev.bKeyDown = (ushort) csi.getValue(3, 0);
    kev.dwControlKeyState = (ushort) csi.getValue(4, 0);
    kev.wRepeatCount = (ushort) csi.getValue(5, 1);

    regenerateMissingScanCodeFromVirtualKeyCode(kev);

    if (kev.bKeyDown && getWin32Key(kev, ev, state))
    {
        TermIO::normalizeKey(ev.keyDown);
        return Accepted;
    }
    return Ignored;
}

// Из-за https://github.com/microsoft/terminal/issues/15083 Conpty
// передаёт управляющие последовательности ANSI внутри событий
// win32-input-mode. Этот класс обеспечивает их правильную обработку.

class Win32InputModeUnwrapper : public InputGetter
{
    InputGetter &in;
    InputState &state;

    enum { maxSize = 31 };

    ushort ungetSize {0};
    short ungetBuffer[maxSize];

public:

    Win32InputModeUnwrapper(InputGetter &aIn, InputState &aState) noexcept :
        in(aIn), state(aState)
    {
    }

    int get() noexcept override
    {
        if (ungetSize > 0)
            return ungetBuffer[--ungetSize];

        GetChBuf buf(in);
        CSIData csi;
        TEvent ev {};
        // Если событие win32-input-mode не содержит скан-кода и содержит
        // однобайтовый символ, берём только символ.
        if ( buf.get() == '\x1B' && buf.get() == '['
             && csi.readFrom(buf) && csi.terminator == '_'
             && parseWin32InputModeKey(csi, ev, state) == Accepted
             && ev.keyDown.charScan.scanCode == 0
             && ev.keyDown.textLength == 1 )
            return (uchar) ev.keyDown.text[0];
        buf.reject();
        return -1;
    }

    void unget(int key) noexcept override
    {
        // Можно восстановить исходное событие win32-input-mode и вызвать
        // in.unget(), но это излишне. Однако символы, возвращённые get(), всё
        // равно нужно временно хранить.
        if (ungetSize < maxSize)
            ungetBuffer[ungetSize++] = (short) key;
    }
};

ParseResult TermIO::parseWin32InputModeKeyOrEscapeSeq(const CSIData &csi, InputGetter &in, TEvent &ev, InputState &state) noexcept
// Предусловие: csi.terminator == '_'.
{
    ParseResult res = parseWin32InputModeKey(csi, ev, state);
    if (res == Accepted && ev.keyDown == 0x001B)
    {
        // Получено начало управляющей последовательности, переданной в событиях
        // win32-input-mode.
        Win32InputModeUnwrapper unwrapper(in, state);
        GetChBuf buf(unwrapper);
        res = parseEscapeSeq(buf, ev, state);
        // Не распространяем Rejected, поскольку использован дополнительный GetChBuf.
        if (res != Accepted)
            res = Ignored;
    }
    return res;
}

static bool setOsc52Clipboard(ConsoleCtl &con, TStringView text, InputState &state) noexcept
{
    TStringView prefix = "\x1B]52;;";
    TStringView suffix = "\x07";
    if (char *buf = (char *) malloc(prefix.size() + suffix.size() + (text.size() * 4)/3 + 4))
    {
        memcpy(buf, prefix.data(), prefix.size());
        TStringView b64 = encodeBase64(text, buf + prefix.size());
        memcpy(buf + prefix.size() + b64.size(), suffix.data(), suffix.size());
        con.write(buf, prefix.size() + b64.size() + suffix.size());
        free(buf);
    }
    // При отсутствии полной поддержки OSC 52 возвращаем false, хотя запрос отправляется
    // всегда. Тогда можно продолжать использовать внутренний буфер обмена.
    return state.hasFullOsc52;
}

static bool requestOsc52Clipboard(ConsoleCtl &con, InputState &state) noexcept
{
    if (state.hasFullOsc52)
    {
        TStringView seq = "\x1B]52;;?\x07";
        con.write(seq.data(), seq.size());
        return true;
    }
    return false;
}

bool TermIO::setClipboardText(ConsoleCtl &con, TStringView text, InputState &state) noexcept
{
    return setFar2lClipboard(con, text, state)
        || setOsc52Clipboard(con, text, state);
}

bool TermIO::requestClipboardText(ConsoleCtl &con, void (&accept)(TStringView), InputState &state) noexcept
{
    state.putPaste = &accept;
    return requestFar2lClipboard(con, state)
        || requestOsc52Clipboard(con, state);
}

char *TermIO::readUntilBelOrSt(GetChBuf &buf) noexcept
// Возвращает строку с завершающим нулём, выделенную malloc, либо null.
{
    size_t capacity = 1024;
    size_t len = 0;
    if (char *s = (char *) malloc(capacity))
    {
        int prev = '\0';
        int c;
        while (c = buf.getUnbuffered(), c != -1)
        {
            if (c == '\x07') // BEL
                break;
            if (c == '\\' && prev == '\x1B') // ST
            {
                len -= (len > 0);
                break;
            }
            if (capacity == len + 1)
            {
                if (void *tmp = realloc(s, capacity *= 2))
                    s = (char *) tmp;
                else
                    s = (free(s), nullptr);
            }
            if (s)
                s[len++] = (char) c;
            prev = c;
        }
        if (s)
            s[len] = '\0';
        return s;
    }
    return {};
}

void TermIO::consumeUnprocessedInput(ConsoleCtl &con, InputGetter &in, InputState &state) noexcept
// Во время выхода терминал может продолжать посылать события. Особенно
// вероятно это при удалённой работе по медленному соединению с
// расширениями отпускания клавиш, например far2l и win32-input-mode,
// либо при завершении сигналом во время перетаскивания мышью. Поэтому
// отправляем DSR и читаем события до ответа. Это делается после
// отключения расширений клавиатуры и мыши.
//
{
    using namespace std::chrono;
    auto timeout = milliseconds(200);

    TStringView seq = "\x1B[6n"; // Отчёт о состоянии устройства, DSR.
    con.write(seq.data(), seq.size());

    TEvent ev {};
    state.gotDsrResponse = false;
    auto begin = steady_clock::now();
    do
    {
        GetChBuf buf {in};
        parseEvent(buf, ev, state);
    }
    while ( !state.gotDsrResponse &&
            (steady_clock::now() - begin <= timeout) );
}

} // пространство имён tvision
