/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   TTEXT.H                                                               */
/*                                                                         */
/* Определяет функции работы с многобайтовыми строками. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if defined( Uses_TText ) && !defined( __TText )
#define __TText

struct TTextMetrics
{
    uint width;
    uint characterCount;
    uint graphemeCount; // Число некомбинируемых символов.
};

class TText
{
public:

    // Возвращает ширину text.
    static size_t width(TStringView text) noexcept;

    // Возвращает ширину, число символов и число графем в text.
    static TTextMetrics measure(TStringView text) noexcept;

    // Возвращает длину первого символа text в байтах.
    static size_t next(TStringView text) noexcept;

    // Увеличивает index на длину в байтах символа text, начинающегося в
    // позиции index. Во втором варианте также увеличивает width на ширину
    // символа. Возвращает признак изменения index.
    //
    static Boolean next(TStringView text, size_t &index) noexcept;
    static Boolean next(TStringView text, size_t &index, size_t &width) noexcept;

    // Возвращает длину в байтах символа text непосредственно перед позицией
    // index.
    static size_t prev(TStringView text, size_t index) noexcept;

    // Сравнивает строки без учёта регистра, включая символы расширенной
    // ASCII.
    static Boolean equalsIgnoreCase(TStringView a, TStringView b) noexcept;

    // Преобразует первый символ text в символ кодовой страницы. Если
    // преобразование невозможно или text пуст, возвращает '\0'.
    static char toCodePage(TStringView text) noexcept;

#if !defined(__BORLANDC__)
    // Преобразует символ кодовой страницы c в UTF-8. Возвращённое представление
    // действительно до следующего вызова setCodePageTranslation().
    static TStringView fromCodePage(char c) noexcept;

    // Изменяет внутреннюю таблицу преобразования символов кодовой страницы в
    // UTF-8 и обратно. По умолчанию используется кодовая страница 437 (США).
    // Нулевой указатель восстанавливает таблицу по умолчанию.
    //
    static void setCodePageTranslation(const char (*utf8Translation)[256][4]) noexcept;
#endif // __BORLANDC__

    // Возвращает длину в байтах начальной подстроки text шириной count
    // столбцов. Если ширина text меньше count, возвращает text.size().
    // Отрицательный count считается нулём. Во втором варианте длина и ширина
    // передаются через выходные параметры length и width. Если столбец count
    // попадает внутрь символа двойной ширины, includeIncomplete определяет,
    // включать ли этот символ в результат.
    //
    //
    static size_t scroll(TStringView text, int count, Boolean includeIncomplete) noexcept;
    static void scroll( TStringView text, int count, Boolean includeIncomplete,
                        size_t &length, size_t &width ) noexcept;

    // Заполняет cells символом c. Во втором варианте также устанавливает
    // атрибут цвета attr.
    static void drawChar(TSpan<TScreenCell> cells, char c) noexcept;
    static void drawChar(TSpan<TScreenCell> cells, char c, TColorAttr attr) noexcept;

    // Копирует текст text в cells, начиная с позиции indent в cells и
    // столбца textIndent в text. Возвращает число заполненных ячеек. В
    // вариантах 2 и 4 также устанавливает атрибут attr; в вариантах 3 и 4
    // используются indent = 0 и textIndent = 0. Если textIndent попадает
    // внутрь символа двойной ширины, вместо него записывается пробел.
    //
    //
    static size_t drawStr( TSpan<TScreenCell> cells, size_t indent,
                           TStringView text, int textIndent ) noexcept;
    static size_t drawStr( TSpan<TScreenCell> cells, size_t indent,
                           TStringView text, int textIndent,
                           TColorAttr attr ) noexcept;
    static size_t drawStr(TSpan<TScreenCell> cells, TStringView text) noexcept;
    static size_t drawStr(TSpan<TScreenCell> cells, TStringView text, TColorAttr attr) noexcept;

    // Читает один символ многобайтовой строки и записывает в ячейку экрана.
    // cells — диапазон ячеек TScreenCell; i — входной и выходной индекс в
    // cells, увеличиваемый на число обновлённых ячеек; text — исходный
    // текст; j — входной и выходной индекс в text, увеличиваемый на число
    // прочитанных байтов. Во втором варианте attr задаёт цветовой атрибут.
    // Возвращает false, когда i >= cells.size() && j >= text.size(). Ячейка
    // может содержать один печатный символ шириной не менее одного столбца и
    // несколько комбинируемых символов нулевой ширины. Если в text найден
    // символ нулевой ширины и i > 0, он присоединяется к cells[i - 1]. При
    // этом i не увеличивается, атрибут цвета не устанавливается.
    //
    //
    //
    //
    //
    //
    //
    static Boolean drawOne( TSpan<TScreenCell> cells, size_t &i,
                            TStringView text, size_t &j ) noexcept;
    static Boolean drawOne( TSpan<TScreenCell> cells, size_t &i,
                            TStringView text, size_t &j,
                            TColorAttr attr ) noexcept;

#ifndef __BORLANDC__
    // Варианты функций выше, использующие обратный вызов '(TColorAttr &) ->
    // void' для изменения атрибута каждой ячейки вместо присвоения
    // фиксированного значения.
    template <class Func>
    static void drawCharEx(TSpan<TScreenCell> cells, char c, Func &&transformAttr) noexcept;
    template <class Func>
    static size_t drawStrEx( TSpan<TScreenCell> cells, size_t indent,
                             TStringView text, int textIndent,
                             Func &&transformAttr ) noexcept;

    // Варианты некоторых функций выше для UTF-32.
    static Boolean next(TSpan<const uint32_t> text, size_t &index, size_t &width) noexcept;
    static void scroll( TSpan<const uint32_t> text, int count, Boolean includeIncomplete,
                        size_t &length, size_t &width ) noexcept;
    static size_t drawStr( TSpan<TScreenCell> cells, size_t indent,
                           TSpan<const uint32_t> text, int textIndent ) noexcept;
    static size_t drawStr( TSpan<TScreenCell> cells, size_t indent,
                           TSpan<const uint32_t> text, int textIndent,
                           TColorAttr attr ) noexcept;
    template <class Func>
    static size_t drawStrEx( TSpan<TScreenCell> cells, size_t indent,
                             TSpan<const uint32_t> text, int textIndent,
                             Func &&transformAttr ) noexcept;
    static Boolean drawOne( TSpan<TScreenCell> cells, size_t &i,
                            TSpan<const uint32_t> text, size_t &j ) noexcept;
    static Boolean drawOne( TSpan<TScreenCell> cells, size_t &i,
                            TSpan<const uint32_t> text, size_t &j,
                            TColorAttr ) noexcept;
private:

    struct Lw { size_t length, width; };
    static Lw nextImpl(TStringView) noexcept;
    static Lw nextImpl(TSpan<const uint32_t>) noexcept;
    static Lw scrollImpl(TStringView, int, Boolean) noexcept;
    static Lw scrollImpl(TSpan<const uint32_t>, int, Boolean) noexcept;
    static Lw drawOneImpl(TSpan<TScreenCell>, size_t, TStringView, size_t) noexcept;
    static Lw drawOneImpl(TSpan<TScreenCell>, size_t, TSpan<const uint32_t>, size_t) noexcept;

    template <class Text>
    static Lw scrollImplT(Text text, int, Boolean) noexcept;
    template <class Text, class Func>
    static size_t drawStrExT(TSpan<TScreenCell>, size_t, Text, int, Func &&) noexcept;
    template <class Text, class Func>
    static Boolean drawOneT(TSpan<TScreenCell>, size_t &i, Text, size_t &, Func &&) noexcept;
#endif // __BORLANDC__
};

#ifdef __BORLANDC__

inline size_t TText::width(TStringView text)
{
    return text.size();
}

#pragma warn -inl

inline TTextMetrics TText::measure(TStringView text)
{
    TTextMetrics metrics = {text.size(), text.size(), text.size()};
    return metrics;
}

#pragma warn .inl

inline size_t TText::next(TStringView text)
{
    return text.size() ? 1 : 0;
}

inline Boolean TText::next(TStringView text, size_t &index)
{
    if (index < text.size())
        return ++index, True;
    return False;
}

inline Boolean TText::next(TStringView text, size_t &index, size_t &width)
{
    if (index < text.size())
        return ++index, ++width, True;
    return False;
}

inline size_t TText::prev(TStringView text, size_t index)
{
    return 0 < index && index <= text.size();
}

inline Boolean TText::equalsIgnoreCase(TStringView a, TStringView b)
{
    if (a.size() != b.size())
        return False;
    return Boolean( a.empty() || memicmp(a.begin(), b.begin(), a.size()) == 0 );
}

inline char TText::toCodePage(TStringView text)
{
    if (text.empty())
        return '\0';
    return text[0];
}

inline size_t TText::scroll(TStringView text, int count, Boolean)
{
    return count > 0 ? min(count, text.size()) : 0;
}

inline void TText::scroll( TStringView text, int count, Boolean,
                           size_t &length, size_t &width )
{
    length = width = (size_t) scroll(text, count, False);
}

#pragma warn -inl

inline void TText::drawChar(TSpan<TScreenCell> cells, char c)
{
    for (size_t i = 0; i < cells.size(); ++i)
        cells[i].character = c;
}

inline void TText::drawChar(TSpan<TScreenCell> cells, char c, TColorAttr attr)
{
    for (size_t i = 0; i < cells.size(); ++i)
        cells[i] = ushort(uchar(c) | (attr << 8));
}

inline size_t TText::drawStr( TSpan<TScreenCell> cells, size_t indent,
                              TStringView text, int textIndent )
{
    // Оптимизация BCC: вместо min/substr/subspan используем цикл по диапазону указателей.
    if (indent < cells.size() && textIndent < text.size())
    {
        size_t count = cells.size() - indent;
        if (text.size() < count)
            count = text.size();

        TScreenCell *dest = &cells[indent];
        const char *src = &text[textIndent];
        const char *end = &text[textIndent + count];
        for (; src != end; ++dest, ++src)
            *(uchar *)dest = *src;

        return count;
    }
    return 0;
}

inline size_t TText::drawStr( TSpan<TScreenCell> cells, size_t indent,
                              TStringView text, int textIndent,
                              TColorAttr attr )
{
    // Оптимизация BCC: вместо min/substr/subspan используем цикл по диапазону указателей.
    if (indent < cells.size() && textIndent < text.size())
    {
        size_t count = cells.size() - indent;
        if (text.size() < count)
            count = text.size();

        TScreenCell *dest = &cells[indent];
        const char *src = &text[textIndent];
        const char *end = &text[textIndent + count];
        for (; src != end; ++dest, ++src)
        {
            ((uchar *) dest)[0] = *src;
            ((uchar *) dest)[1] = attr;
        }

        return count;
    }
    return 0;
}

#pragma warn .inl

inline size_t TText::drawStr(TSpan<TScreenCell> cells, TStringView text)
{
    return drawStr(cells, 0, text, 0);
}

inline size_t TText::drawStr( TSpan<TScreenCell> cells, TStringView text,
                              TColorAttr attr )
{
    return drawStr(cells, 0, text, 0, attr);
}

inline Boolean TText::drawOne( TSpan<TScreenCell> cells, size_t &i,
                               TStringView text, size_t &j )
{
    if (i < cells.size() && j < text.size())
    {
        cells[i++].character = text[j++];
        return True;
    }
    return False;
}

inline Boolean TText::drawOne( TSpan<TScreenCell> cells, size_t &i,
                               TStringView text, size_t &j,
                               TColorAttr attr )
{
    if (i < cells.size() && j < text.size())
    {
        cells[i++] = ushort(uchar(text[j++]) | (attr << 8));
        return True;
    }
    return False;
}

#else

inline Boolean TText::next(TStringView text, size_t &index) noexcept
{
    size_t len = TText::next(text.substr(index));
    index += len;
    return len != 0;
}

inline Boolean TText::next(TStringView text, size_t &index, size_t &width) noexcept
{
    auto result = TText::nextImpl(text.substr(index));
    index += result.length;
    width += result.width;
    return result.length != 0;
}

inline Boolean TText::next(TSpan<const uint32_t> text, size_t &index, size_t &width) noexcept
{
    auto result = TText::nextImpl(text.subspan(index));
    index += result.length;
    width += result.width;
    return result.length != 0;
}

inline size_t TText::scroll(TStringView text, int count, Boolean includeIncomplete) noexcept

{
    size_t length = 0, width = 0;
    scroll(text, count, includeIncomplete, length, width);
    return length;
}

inline void TText::scroll( TStringView text, int count, Boolean includeIncomplete,
                           size_t &length, size_t &width ) noexcept

{
    auto result = scrollImpl(text, count, includeIncomplete);
    length = result.length;
    width = result.width;
}

inline void TText::scroll( TSpan<const uint32_t> text, int count, Boolean includeIncomplete,
                           size_t &length, size_t &width ) noexcept

{
    auto result = scrollImpl(text, count, includeIncomplete);
    length = result.length;
    width = result.width;
}

inline void TText::drawChar(TSpan<TScreenCell> cells, char c) noexcept
{
    drawCharEx(cells, c, [] (auto &) {});
}

inline void TText::drawChar(TSpan<TScreenCell> cells, char c, TColorAttr attr) noexcept
{
    TScreenCell aCell {c, attr};
    for (auto &cell : cells)
        cell = aCell;
}

template <class Func>
inline void TText::drawCharEx(TSpan<TScreenCell> cells, char ch, Func &&transformAttr) noexcept
{
    for (auto &c : cells)
    {
        c.character = ch;
        transformAttr(c.attribute);
    }
}

inline size_t TText::drawStr(TSpan<TScreenCell> cells, TStringView text) noexcept
{
    return drawStr(cells, 0, text, 0);
}

inline size_t TText::drawStr( TSpan<TScreenCell> cells, TStringView text,
                              TColorAttr attr ) noexcept
{
    return drawStr(cells, 0, text, 0, attr);
}

inline size_t TText::drawStr( TSpan<TScreenCell> cells, size_t indent,
                              TStringView text, int textIndent ) noexcept
{
    return drawStrEx(cells, indent, text, textIndent, [] (auto &) {});
}

inline size_t TText::drawStr( TSpan<TScreenCell> cells, size_t indent,
                              TSpan<const uint32_t> text, int textIndent ) noexcept
{
    return drawStrEx(cells, indent, text, textIndent, [] (auto &) {});
}

inline size_t TText::drawStr( TSpan<TScreenCell> cells, size_t indent,
                              TStringView text, int textIndent,
                              TColorAttr aAttr ) noexcept
{
    return drawStrEx(cells, indent, text, textIndent, [&] (auto &attr) {
        attr = aAttr;
    });
}

inline size_t TText::drawStr( TSpan<TScreenCell> cells, size_t indent,
                              TSpan<const uint32_t> text, int textIndent,
                              TColorAttr aAttr ) noexcept
{
    return drawStrEx(cells, indent, text, textIndent, [&] (auto &attr) {
        attr = aAttr;
    });
}

template <class Func>
inline size_t TText::drawStrEx( TSpan<TScreenCell> cells, size_t indent,
                                TStringView text, int textIndent,
                                Func &&transformAttr ) noexcept
{
    return drawStrExT(cells, indent, text, textIndent, (Func &&) transformAttr);
}

template <class Func>
inline size_t TText::drawStrEx( TSpan<TScreenCell> cells, size_t indent,
                                TSpan<const uint32_t> text, int textIndent,
                                Func &&transformAttr ) noexcept
{
    return drawStrExT(cells, indent, text, textIndent, (Func &&) transformAttr);
}

inline Boolean TText::drawOne( TSpan<TScreenCell> cells, size_t &i,
                               TStringView text, size_t &j ) noexcept
{
    return drawOneT(cells, i, text, j, [] (auto &) {});
}

inline Boolean TText::drawOne( TSpan<TScreenCell> cells, size_t &i,
                               TSpan<const uint32_t> text, size_t &j ) noexcept
{
    return drawOneT(cells, i, text, j, [] (auto &) {});
}

inline Boolean TText::drawOne( TSpan<TScreenCell> cells, size_t &i,
                               TStringView text, size_t &j,
                               TColorAttr aAttr ) noexcept
{
    return drawOneT(cells, i, text, j, [&] (auto &attr) {
        attr = aAttr;
    });
}

inline Boolean TText::drawOne( TSpan<TScreenCell> cells, size_t &i,
                               TSpan<const uint32_t> text, size_t &j,
                               TColorAttr aAttr ) noexcept
{
    return drawOneT(cells, i, text, j, [&] (auto &attr) {
        attr = aAttr;
    });
}

template <class Text, class Func>
inline size_t TText::drawStrExT( TSpan<TScreenCell> cells, size_t indent,
                                 Text text, int textIndent,
                                 Func &&transformAttr ) noexcept
{
    size_t i = indent, j = 0;
    if (textIndent > 0)
    {
        size_t leadWidth;
        TText::scroll(text, textIndent, True, j, leadWidth);
        if (leadWidth > (size_t) textIndent && i < cells.size())
        {
            cells[i].character = ' ';
            transformAttr(cells[i++].attribute);
        }
    }
    while (TText::drawOneT(cells, i, text, j, (Func &&) transformAttr));
    return i - indent;
}

template <class Text, class Func>
inline Boolean TText::drawOneT( TSpan<TScreenCell> cells, size_t &i,
                                Text text, size_t &j, Func &&transformAttr ) noexcept
{
    auto result = drawOneImpl(cells, i, text, j);
    if (result.width)
        transformAttr(cells[i].attribute);
    if (result.width > 1)
        transformAttr(cells[i + 1].attribute);
    i += result.width;
    j += result.length;
    return result.length != 0;
}

#endif // __BORLANDC__

#endif // Uses_TText
