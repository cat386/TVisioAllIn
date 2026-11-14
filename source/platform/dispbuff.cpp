#define Uses_TScreenCell
#include <tvision/tv.h>

#include <internal/dispbuff.h>
#include <internal/platform.h>
#include <internal/codepage.h>
#include <chrono>

#ifdef _MSC_VER
#define __builtin_expect(x, y) x
#endif

namespace tvision
{

DisplayBuffer::DisplayBuffer(int aMaxFps) noexcept :
    // Это можно проверять во время выполнения, но пока автору известно лишь это.
#ifdef _WIN32
    wideOverlapping(false)
#else
    wideOverlapping(true)
#endif
{
    maxFps = aMaxFps < 0 ? defaultFps : aMaxFps;
    if (maxFps > 0)
        flushDelay = std::chrono::microseconds((int) 1e6/maxFps);
}

TScreenCell *DisplayBuffer::reloadScreenInfo(DisplayAdapter &display) noexcept
{
    size = display.reloadScreenInfo();
    caretSize = -1;
    resizeBuffer();
    return buffer.data();
}

void DisplayBuffer::resizeBuffer() noexcept
{
    for (auto *buf : {&buffer, &flushBuffer})
    {
        buf->resize(0);
        buf->resize(size.x*size.y); // Инициализировано нулями.
    }

    rowDamage.resize(0);
    rowDamage.resize(size.y, {INT_MAX, INT_MIN});
}

void DisplayBuffer::clearScreen(DisplayAdapter &display) noexcept
{
    display.clearScreen();
    display.flush();
    resizeBuffer();
}

void DisplayBuffer::redrawScreen(DisplayAdapter &display) noexcept
{
    screenTouched = true;
    lastFlush = {};
    memset(&flushBuffer[0], 0, flushBuffer.size()*sizeof(TScreenCell));
    for (auto &range : rowDamage)
        range = {0, size.x - 1};
    flushScreen(display);
}

void DisplayBuffer::setCaretSize(int size) noexcept
{
    newCaretSize = size;
}

void DisplayBuffer::setCaretPosition(int x, int y) noexcept
{
    caretPosition = {x, y};
    caretOrCursorChanged = true;
}

void DisplayBuffer::screenWrite(int x, int y, TScreenCell *buf, int len) noexcept
{
    if (inBounds(x, y) && len > 0)
    {
        len = min(len, size.x - x);
        TScreenCell *dst = &buffer[y*size.x + x];
        // buffer также служит TScreen::screenBuffer и непосредственно изменяется
        // TView::writeView. В большинстве случаев копирование не требуется:
        // данные уже на месте.
        if (buf == dst)
            ; // Копировать ничего не нужно. Но TScreen::screenBuffer открыт, поэтому
        // теоретически исходная и целевая области могут перекрываться. Turbo
        // Vision так не делает, однако обрабатываем и этот случай.
        //
        else if (max(buf, dst) <= min(&buf[len], &dst[len]))
            memmove(dst, buf, len*sizeof(TScreenCell)); // Копируем между перекрывающимися областями.
        else
            memcpy(dst, buf, len*sizeof(TScreenCell));

        setDirty(x, y, len);
        screenTouched = true;
    }
}

void DisplayBuffer::setDirty(int x, int y, int len) noexcept
{
    Range dam = rowDamage[y];
    if (x < dam.begin)
        dam.begin = x;
    if (x + len - 1 > dam.end)
        dam.end = x + len - 1;
    rowDamage[y] = dam;
}

bool DisplayBuffer::timeToFlush() noexcept
{
    // Не обновляем экран чаще максимального FPS.
    if (maxFps > 0)
    {
        auto now = Clock::now();
        auto flushTime = lastFlush + flushDelay;
        if (flushTime <= now)
        {
            lastFlush = now;
            pendingFlush = TimePoint();
        }
        else
        {
            pendingFlush = flushTime;
            return false;
        }
    }
    return true;
}

int DisplayBuffer::timeUntilPendingFlushMs() noexcept
{
    using namespace std::chrono;
    if (pendingFlush == TimePoint())
        return -1;
    auto now = Clock::now();
    if (pendingFlush < now)
        return 0;
    return duration_cast<milliseconds>(pendingFlush - now).count();
}

void DisplayBuffer::setCursorPosition(int x, int y) noexcept
{
    TPoint pos {x, y};
    if (cursorVisible && cursorPosition != pos)
        caretOrCursorChanged = true;
    cursorPosition = pos;
}

void DisplayBuffer::setCursorVisibility(bool visible) noexcept
{
    if (cursorVisible != visible)
        caretOrCursorChanged = true;
    cursorVisible = visible;
}

static uchar negateAttribute(TColorAttr attr) noexcept
{
    return attr.toBIOS() ^ 0x77;
}

void DisplayBuffer::drawCursor() noexcept
{
    if (cursorVisible)
    {
        auto x = cursorPosition.x, y = cursorPosition.y;
        if (inBounds(x, y))
        {
            auto *cell = &buffer[y*size.x + x];
            if ( cell->character.isWideCharTrail() &&
                 x > 0 && (cell - 1)->character.isWide() )
                --cell, --x;
            attrUnderCursor = cell->attribute;
            cell->attribute = negateAttribute(cell->attribute);
            setDirty(x, y, 1);
        }
    }
}

void DisplayBuffer::undrawCursor() noexcept
{
    if (cursorVisible)
    {
        auto x = cursorPosition.x, y = cursorPosition.y;
        if (inBounds(x, y))
        {
            auto *cell = &buffer[y*size.x + x];
            if ( cell->character.isWideCharTrail() &&
                 x > 0 && (cell - 1)->character.isWide() )
                --cell, --x;
            cell->attribute = attrUnderCursor;
            setDirty(x, y, 1);
        }
    }
}

bool DisplayBuffer::needsFlush() const noexcept
{
    return screenTouched || caretOrCursorChanged || caretSize != newCaretSize;
}

namespace
{
void flushScreenAlgorithm(DisplayBuffer &, DisplayAdapter &) noexcept;
}

void DisplayBuffer::flushScreen(DisplayAdapter &display) noexcept
{
    if (needsFlush() && timeToFlush())
    {
        drawCursor();
        flushScreenAlgorithm(*this, display);
        if (caretPosition.x != -1)
            display.setCaretPosition(caretPosition);
        undrawCursor();
        if (caretSize != newCaretSize)
            display.setCaretSize(newCaretSize);
        display.flush();
        screenTouched = false;
        caretOrCursorChanged = false;
        caretSize = newCaretSize;
    }
}

// Алгоритм обновления экрана FlushScreenAlgorithm.
//

namespace
{

struct FlushScreenAlgorithm
{
    DisplayBuffer &disp;
    DisplayAdapter &display;
    TPoint size;
    int x, y, rowOffs;
    TScreenCell *cell;
    DisplayBuffer::Range damage;

    const TScreenCell &cellAt(int x) const noexcept;
    void getCell() noexcept;
    bool cellDirty() const noexcept;
    bool wideCanOverlap() const noexcept;

    void run() noexcept;
    void processCell() noexcept;
    void writeCell() noexcept;
    void writeSpace() noexcept;
    void writeCell(const TScreenCharacter &Char, const TColorAttr &Attr, bool wide) noexcept;
    void commitDirty() noexcept;
    void ensureCharacterIsPrintable(TScreenCell &cell) const noexcept;
    void handleWideCharSpill() noexcept;
    void handleTrail() noexcept;
};

inline bool isTrail(const TScreenCell &cell) noexcept
{
    return __builtin_expect(cell.character.isWideCharTrail(), 0);
}

inline bool isWide(const TScreenCell &cell) noexcept
{
    return __builtin_expect(cell.character.isWide(), 0);
}

inline const TScreenCell& FlushScreenAlgorithm::cellAt(int x) const noexcept
{
    return disp.buffer[rowOffs + x];
}

inline void FlushScreenAlgorithm::getCell() noexcept
{
    cell = &disp.buffer[rowOffs + x];
    ensureCharacterIsPrintable(*cell);
}

inline bool FlushScreenAlgorithm::cellDirty() const noexcept
{
    return *cell != disp.flushBuffer[cell - &disp.buffer[0]];
}

inline void FlushScreenAlgorithm::commitDirty() noexcept
{
    disp.flushBuffer[cell - &disp.buffer[0]] = *cell;
}

inline bool FlushScreenAlgorithm::wideCanOverlap() const noexcept
{
    return disp.wideOverlapping;
}

inline void flushScreenAlgorithm(DisplayBuffer &disp, DisplayAdapter &display) noexcept
{
    FlushScreenAlgorithm {disp, display}.run();
}

inline void FlushScreenAlgorithm::run() noexcept
{
    size = disp.size;
    rowOffs = 0;
    for (y = 0; y < size.y; ++y, rowOffs += size.x)
    {
        damage = disp.rowDamage[y];
        if (damage.begin <= damage.end)
        {
            x = damage.begin;
            // Если перекрытие запрещено, проверяем широкий символ перед первой
            // позицией вывода.
            bool wideSpillBefore = !wideCanOverlap() && 0 < x && isWide(cellAt(x-1));
            for (; x <= damage.end; ++x)
            {
                getCell();
                if (cellDirty() || isTrail(*cell)) {
                    if (wideSpillBefore) {
                        --x;
                        getCell();
                        wideSpillBefore = false;
                    }
                    commitDirty();
                    processCell();
                } else
                    wideSpillBefore = !wideCanOverlap() && isWide(*cell);
            }
            if (x < size.x) {
                getCell();
                if (isTrail(*cell))
                    // Начало широкого символа перезаписано; атрибуты могут распространиться
                    // на оставшиеся ячейки продолжения.
                    handleTrail();
            }
        }
        disp.rowDamage[y] = {INT_MAX, INT_MIN};
    }
}

inline void FlushScreenAlgorithm::processCell() noexcept
{
    if (isWide(*cell)) {
        handleWideCharSpill();
        return;
    } else if (isTrail(*cell)) {
        handleTrail();
        return;
    }
    writeCell();
}

inline void FlushScreenAlgorithm::writeCell() noexcept
{
    writeCell(cell->character, cell->attribute, cell->character.isWide());
}

inline void FlushScreenAlgorithm::writeSpace() noexcept
{
    TScreenCharacter ch;
    ch.initWithChar(' ');
    writeCell(ch, cell->attribute, 0);
}

inline void FlushScreenAlgorithm::writeCell( const TScreenCharacter &ch,
                                             const TColorAttr &attr,
                                             bool wide ) noexcept
{
    display.writeCell({x, y}, ch.getText(), attr, wide);
}

void FlushScreenAlgorithm::ensureCharacterIsPrintable(TScreenCell &cell) const noexcept
{
    if (cell.character.isWideCharTrail())
        return;

    TStringView text = cell.character.getText();
    uchar c = text[0];
    if (c == '\0')
        cell.character.initWithChar(' ');
    else if (text.size() == 1 && (c < ' ' || 0x7F <= c))
        // В качестве запасного варианта преобразуем из кодовой страницы.
        cell.character.initWithMultiByteChar(CpTranslator::toPackedUtf8(c));
}

void FlushScreenAlgorithm::handleWideCharSpill() noexcept
{
    uchar width = cell->character.isWide();
    const auto Attr = cell->attribute;
    if (x + width < size.x)
        writeCell();
    else {
        // Заменяем пробелами, если иначе символ будет выведен на следующей строке.
        writeSpace();
        while (--width && ++x < size.x) {
            getCell();
            if (!isTrail(*cell)) {
                --x;
                return;
            }
            writeSpace();
        }
        return;
    }
    // Не допускаем выхода символа в следующие ячейки.
    int wbegin = x;
    while (width-- && ++x < size.x) {
        getCell();
        commitDirty();
        if (!isTrail(*cell)) {
            if (wideCanOverlap()) {
                // Перезаписываем широкий символ.
                writeCell();
            } else {
                // Заменяем пробелами весь широкий символ.
                int wend = x;
                x = wbegin;
                do {
                    getCell();
                    writeSpace();
                } while (++x < wend);
                getCell();
                writeCell();
            }
            return;
        }
    }
    // Если у следующего символа другой атрибут, всё равно выводим его,
    // предотвращая распространение атрибута.
    if (x + 1 < size.x) {
        ++x;
        getCell();
        if (Attr != cell->attribute) {
            commitDirty();
            processCell();
        } else
            --x;
    }
}

void FlushScreenAlgorithm::handleTrail() noexcept
{
    const auto Attr = cell->attribute;
    if (x > 0) {
        --x;
        getCell();
        // Проверяем символ за заполнителем.
        if (cell->character.isWide()) {
            handleWideCharSpill();
            return;
        }
        ++x;
        getCell();
    }
    // Последовательные заполнители выводим пробелами.
    do {
        commitDirty();
        writeSpace();
        ++x;
        if (x < size.x)
            getCell();
        else
            return;
    } while (isTrail(*cell));
    // Теперь получен обычный символ.
    if (x > damage.end && Attr != cell->attribute) {
        // Перерисовываем символ, который иначе не выводился бы, чтобы
        // предотвратить распространение атрибута.
        processCell();
    } else {
        // Уменьшаем для следующей итерации.
        --x;
    }

}

} // пространство имён

} // пространство имён tvision
