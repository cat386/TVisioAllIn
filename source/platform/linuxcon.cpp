#ifdef __linux__

#define Uses_TEvent
#define Uses_TKeys
#include <tvision/tv.h>

#include <internal/linuxcon.h>
#include <internal/conctl.h>
#include <internal/gpminput.h>
#include <internal/termio.h>
#include <internal/sigwinch.h>
#include <linux/keyboard.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

namespace tvision
{

inline LinuxConsoleAdapter::LinuxConsoleAdapter( DisplayAdapter &aDisplay,
                                                 LinuxConsoleInput &aWrapper,
                                                 InputState &aInputState,
                                                 SigwinchHandler *aSigwinch,
                                                 GpmInput *aGpm ) noexcept :
    ConsoleAdapter(aDisplay, {&aWrapper, aGpm, aSigwinch}),
    inputState(aInputState),
    sigwinch(aSigwinch),
    wrapper(aWrapper),
    gpm(aGpm)
{
    // Не пропускаем незамеченное изменение размера экрана, например после
    // восстановления из SIGTSTP.
    if (sigwinch)
        sigwinch->signal();
}

LinuxConsoleAdapter &LinuxConsoleAdapter::create( ConsoleCtl &con,
                                                  DisplayBuffer &displayBuf,
                                                  InputState &inputState,
                                                  DisplayAdapter &display,
                                                  InputAdapter &input ) noexcept
{
    auto *sigwinch = SigwinchHandler::create();
    auto &wrapper = *new LinuxConsoleInput(con, input);
    auto *gpm = GpmInput::create(displayBuf);
    return *new LinuxConsoleAdapter(display, wrapper, inputState, sigwinch, gpm);
}

LinuxConsoleAdapter::~LinuxConsoleAdapter()
{
    delete sigwinch;
    delete gpm;
    delete &wrapper.input;
    delete &wrapper;
    delete &display;
    delete &inputState;
}

bool LinuxConsoleInput::getEvent(TEvent &ev) noexcept
{
    // Функция чтения клавиш обычно не знает модификаторы консоли, поэтому
    // дополняем ими результат предыдущего преобразования.
    if (input.getEvent(ev))
    {
        auto &keyCode = ev.keyDown.keyCode;
        auto &controlKeyState = ev.keyDown.controlKeyState;
        controlKeyState = getKeyboardModifiers();
        // Не трактуем Ctrl+H/Ctrl+I/Ctrl+J/Ctrl+M как
        // Ctrl+Back/Ctrl+Tab/Ctrl+Enter.
        if (keyCode == kbBack || keyCode == kbTab || keyCode == kbEnter)
            controlKeyState &= ~kbCtrlShift;
        // Особые случаи Ctrl+Back, Shift+Tab и Alt+Tab.
        if (keyCode == 0x001F && (controlKeyState & kbCtrlShift))
            keyCode = kbCtrlBack;
        else if (keyCode == kbShiftTab || keyCode == kbAltTab)
            keyCode = kbTab; // Учитываем только модификаторы controlKeyState. Если правый Alt создаёт
        // текст, это AltGr и модификатор нужно отбросить.
        if ((controlKeyState & kbRightAlt) && ev.keyDown.textLength != 0)
            controlKeyState &= ~kbRightAlt;
        TermIO::normalizeKey(ev.keyDown);
        return true;
    }
    return false;
}

bool LinuxConsoleInput::hasPendingEvents() noexcept
{
    return input.hasPendingEvents();
}

ushort LinuxConsoleInput::getKeyboardModifiers() noexcept
{
    char res = 6;
    if (ioctl(con.in(), TIOCLINUX, &res) != -1)
        return convertLinuxKeyModifiers(res);
    return 0;
}

ushort LinuxConsoleInput::convertLinuxKeyModifiers(ushort linuxShiftState) noexcept
{
    constexpr struct
    {
        uchar kb;
        uchar tv;
    } linuxModifierFlags[] =
    {
        {1 << KG_SHIFT, kbShift},
        {1 << KG_CTRLL, kbLeftCtrl},
        {1 << KG_CTRLR, kbRightCtrl},
        {1 << KG_ALT, kbLeftAlt},
        {1 << KG_ALTGR, kbRightAlt},
    };

    ushort modifiers = 0;
    for (const auto &flags : linuxModifierFlags)
        if (linuxShiftState & flags.kb)
            modifiers |= flags.tv;
    // Возможно, обнаружен KG_CTRL, но не KG_CTRLL и не KG_CTRLR.
    if ((linuxShiftState & (1 << KG_CTRL)) && !(modifiers & kbCtrlShift))
        modifiers |= kbLeftCtrl;
    return modifiers;
}

} // пространство имён tvision

#endif // __linux__
