#ifndef TVISION_PLATFORM_H
#define TVISION_PLATFORM_H

#define Uses_TPoint
#define Uses_TColorAttr
#include <tvision/tv.h>
#include <internal/dispbuff.h>
#include <internal/events.h>
#include <internal/mutex.h>
#include <vector>

struct TEvent;

namespace tvision
{

class DisplayAdapter
{
public:

    virtual ~DisplayAdapter() {}

    virtual TPoint reloadScreenInfo() noexcept { return {0, 0}; }

    virtual int getColorCount() noexcept { return 0; }
    virtual TPoint getFontSize() noexcept { return {0, 0}; }

    virtual void writeCell( TPoint /* pos */, TStringView /*text*/,
                            TColorAttr /*attr*/, bool /*doubleWidth*/ ) noexcept {}
    virtual void setCaretPosition(TPoint /*pos*/) noexcept {};
    virtual void setCaretSize(int /*size*/) noexcept {};
    virtual void clearScreen() noexcept {}
    virtual void flush() noexcept {};
};

class InputAdapter : public EventSource
{
public:

    InputAdapter(SysHandle aHandle) noexcept :
        EventSource(aHandle)
    {
    }

    virtual ~InputAdapter() {}
};

struct ConsoleAdapter
{
    DisplayAdapter &display;

    const std::vector<EventSource *> sources;

    ConsoleAdapter( DisplayAdapter &aDisplay,
                    std::vector<EventSource *> &&aSources ) noexcept :
        display(aDisplay),
        sources(std::move(aSources))
    {
    }

    virtual ~ConsoleAdapter() {}

    virtual bool isAlive() noexcept { return true; }
    virtual bool setClipboardText(TStringView) noexcept { return false; }
    virtual bool requestClipboardText(void (&)(TStringView)) noexcept { return false; }
};

class Platform
{
    EventWaiter waiter;
    DisplayBuffer displayBuf;
    DisplayAdapter dummyDisplay;
    ConsoleAdapter dummyConsole {dummyDisplay, {}};
    // Инвариант: console содержит либо невладеющую ссылку на dummyConsole,
    // либо владеющую ссылку на объект ConsoleAdapter в куче.
    SignalSafeReentrantMutex<ConsoleAdapter *> console {&dummyConsole};

    static Platform *instance;

    Platform() noexcept;

    void setUpConsole(ConsoleAdapter *&) noexcept;
    void restoreConsole(ConsoleAdapter *&) noexcept;
    void checkConsole() noexcept;
    bool sizeChanged(TEvent &ev) noexcept;
    ConsoleAdapter &createConsole() noexcept;

    static void initLocale() noexcept;
    static void initCharOps() noexcept;
    static int initCharWidth(uint32_t) noexcept;
    static uint32_t initCharToLower(uint32_t) noexcept;

    static void signalCallback(bool) noexcept;

public:

    // Platform — одиночный экземпляр. Он создаётся THardwareInfo и никогда
    // не уничтожается, чтобы вторичные потоки могли продолжать вызывать
    // методы вроде interruptEventWait.
    static Platform &getInstance() noexcept;

    // Функции обработки символов могут зависеть от локали C и других
    // системных ресурсов. При использовании до создания Platform они
    // автоматически выполняют необходимую инициализацию.
    static struct CharOps
    {
        int (*width)(uint32_t) noexcept;
        uint32_t (*toLower)(uint32_t) noexcept;
    } charOps;

    // Примечание: GCC 5 требует явного this.
    void setUpConsole() noexcept
        { console.lock([&] (auto *&c) { this->setUpConsole(c); }); }
    void restoreConsole() noexcept
        { console.lock([&] (auto *&c) { this->restoreConsole(c); }); }

    bool getEvent(TEvent &ev) noexcept { return waiter.getEvent(ev); }
    void waitForEvents(int ms) noexcept;
    void interruptEventWait() noexcept { waiter.interruptEventWait(); }

    // Ограничиваем размер курсора диапазоном от 1 до 100: так работает
    // исходный THardwareInfo::getCaretSize(), и этого ожидает TScreen.
    int getCaretSize() noexcept { return min(max(displayBuf.caretSize, 1), 100); }
    bool isCaretVisible() noexcept { return displayBuf.caretSize > 0; }
    void clearScreen() noexcept
        { console.lock([&] (auto *c) { displayBuf.clearScreen(c->display); }); }
    int getScreenRows() noexcept { return displayBuf.size.y; }
    int getScreenCols() noexcept { return displayBuf.size.x; }
    void setCaretPosition(int x, int y) noexcept { displayBuf.setCaretPosition(x, y); }
    ushort getScreenMode() noexcept;
    void setCaretSize(int size) noexcept { displayBuf.setCaretSize(size); }
    void screenWrite(int x, int y, TScreenCell *b, int l) noexcept { displayBuf.screenWrite(x, y, b, l); }
    void flushScreen() noexcept
        { console.lock([&] (auto *c) { displayBuf.flushScreen(c->display); }); }
    TScreenCell *reloadScreenInfo() noexcept
        { return console.lock([&] (auto *c) { return displayBuf.reloadScreenInfo(c->display); }); }
    void freeScreenBuffer() noexcept { displayBuf.reset(); }

    bool setClipboardText(TStringView text) noexcept
        { return console.lock([&] (auto *c) { return c->setClipboardText(text); }); }
    bool requestClipboardText(void (&accept)(TStringView)) noexcept
        { return console.lock([&] (auto *c) { return c->requestClipboardText(accept); }); }
};

} // пространство имён tvision

#endif // TVISION_PLATFORM_H
