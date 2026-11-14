/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   COLORS.H                                                              */
/*                                                                         */
/* Определяет классы TColorBIOS, TColorRGB, TColorXTerm, */
/* TColor, TColorAttr и TAttrPair. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#ifndef TVISION_COLORS_H
#define TVISION_COLORS_H

#if defined( __BORLANDC__ )

// Байт цветового атрибута BIOS. Младшие четыре бита задают цвет текста,
// старшие четыре — цвет фона. В зависимости от видеорежима старший бит
// означает яркость фона либо мигание.
//

typedef uchar TColorAttr;

// Пара цветовых атрибутов.

typedef ushort TAttrPair;

#else

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* класс TColorRGB */
/*                                                                         */
/* Представляет 24-битный цвет RGB. */
/*                                                                         */
/* Пример инициализации: */
/* TColorRGB rgb = {127, 0, 187}; // {красный, зелёный, синий}. */
/* Либо целым числом: */
/*      TColorRGB rgb = 0x7F00BB;      // 0xRRGGBB                         */
/* Можно преобразовать обратно в целочисленный тип: */
/*      uint32_t asInt = TColorRGB(127, 0, 187);                           */
/* При этом неиспользуемые биты отбрасываются: */
/*      uint32_t(TColorRGB(0xAABBCCDD)) == 0xBBCCDD;                       */
/*                                                                         */
/*-------------------------------------------------------------------------*/

class TColorRGB
{
public:

    constexpr TColorRGB() noexcept;
    constexpr TColorRGB(uint8_t r, uint8_t g, uint8_t b) noexcept;
    constexpr TColorRGB(uint32_t rgb) noexcept;

    constexpr uint8_t getRed() const noexcept;
    constexpr void setRed(uint8_t r) noexcept;
    constexpr uint8_t getGreen() const noexcept;
    constexpr void setGreen(uint8_t g) noexcept;
    constexpr uint8_t getBlue() const noexcept;
    constexpr void setBlue(uint8_t b) noexcept;

    constexpr operator uint32_t() const noexcept;

private:

    uint32_t
        _b      : 8,
        _g      : 8,
        _r      : 8,
        _unused : 8;
};

constexpr TColorRGB::TColorRGB() noexcept :
    TColorRGB(0)
{
    (void) _unused;
}

constexpr TColorRGB::TColorRGB(uint8_t r, uint8_t g, uint8_t b) noexcept :
    _b(b),
    _g(g),
    _r(r),
    _unused(0)
{
}

constexpr TColorRGB::TColorRGB(uint32_t rgb) noexcept :
    _b(uint8_t(rgb)),
    _g(uint8_t(rgb >> 8)),
    _r(uint8_t(rgb >> 16)),
    _unused(0)
{
}

constexpr uint8_t TColorRGB::getRed() const noexcept
{
    return _r;
}

constexpr void TColorRGB::setRed(uint8_t r) noexcept
{
    _r = r;
}

constexpr uint8_t TColorRGB::getGreen() const noexcept
{
    return _g;
}

constexpr void TColorRGB::setGreen(uint8_t g) noexcept
{
    _g = g;
}

constexpr uint8_t TColorRGB::getBlue() const noexcept
{
    return _b;
}

constexpr void TColorRGB::setBlue(uint8_t b) noexcept
{
    _b = b;
}

constexpr TColorRGB::operator uint32_t() const noexcept
{
    return (_r << 8 | _g) << 8 | _b;
}

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* класс TColorBIOS */
/*                                                                         */
/* Эта 4-битная кодировка цвета изначально применялась в Turbo Vision для */
/*  DOS.                                                                   */
/*                                                                         */
/* В эмуляторах терминала цвета BIOS отображаются с помощью 16 базовых */
/* цветов ANSI. */
/*                                                                         */
/* Тип допускает преобразование в целочисленные типы и обратно: */
/*      TColorBIOS black = 0x0,                                            */
/*                 blue = 0x1,                                             */
/*                 dark_gray = 0x8;                                        */
/*      uint8_t asChar = TColorBIOS(0xC);                                  */
/* При этом неиспользуемые биты отбрасываются: */
/*      uint8_t(TColorBIOS(0xAB)) == 0xB;                                  */
/*                                                                         */
/*-------------------------------------------------------------------------*/

class TColorBIOS
{
public:

    constexpr TColorBIOS() noexcept;
    constexpr TColorBIOS(uint8_t irgb) noexcept;

    constexpr bool getRed() const noexcept;
    constexpr void setRed(bool r) noexcept;
    constexpr bool getGreen() const noexcept;
    constexpr void setGreen(bool g) noexcept;
    constexpr bool getBlue() const noexcept;
    constexpr void setBlue(bool b) noexcept;
    constexpr bool getIntensity() const noexcept;
    constexpr void setIntensity(bool i) noexcept;

    constexpr operator uint8_t() const noexcept;

private:

    uint8_t
        _b       : 1,
        _g       : 1,
        _r       : 1,
        _i       : 1,
        _unused  : 4;
};

constexpr TColorBIOS::TColorBIOS() noexcept :
    TColorBIOS(0)
{
    (void) _unused;
}

constexpr TColorBIOS::TColorBIOS(uint8_t irgb) noexcept :
    _b(irgb),
    _g(irgb >> 1),
    _r(irgb >> 2),
    _i(irgb >> 3),
    _unused(0)
{
}

constexpr bool TColorBIOS::getRed() const noexcept
{
    return _r;
}

constexpr void TColorBIOS::setRed(bool r) noexcept
{
    _r = r;
}

constexpr bool TColorBIOS::getGreen() const noexcept
{
    return _g;
}

constexpr void TColorBIOS::setGreen(bool g) noexcept
{
    _g = g;
}

constexpr bool TColorBIOS::getBlue() const noexcept
{
    return _b;
}

constexpr void TColorBIOS::setBlue(bool b) noexcept
{
    _b = b;
}

constexpr bool TColorBIOS::getIntensity() const noexcept
{
    return _i;
}

constexpr void TColorBIOS::setIntensity(bool i) noexcept
{
    _i = i;
}

constexpr TColorBIOS::operator uint8_t() const noexcept
{
    return ((_i << 1 | _r) << 1 | _g) << 1 | _b;
}

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* класс TColorXTerm */
/*                                                                         */
/* Индекс в 256-цветной палитре, распознаваемой типом терминала 'xterm-256color'. */
/* Некоторые эмуляторы позволяют изменять эту палитру, */
/* но Turbo Vision не использует такую возможность. */
/*                                                                         */
/* Индексы 0–15 — базовые цвета ANSI. Они отображаются */
/* в соответствии с цветовой схемой эмулятора терминала. */
/* Индексы 16–231 образуют куб RGB размером 6×6×6. */
/* Индексы 232–255 — шкала из 24 оттенков серого. */
/*                                                                         */
/* Тип допускает преобразование в целочисленные типы и обратно: */
/*      TColorXTerm xterm = 0xFE;                                          */
/*      uint8_t asChar = xterm;                                            */
/*                                                                         */
/*-------------------------------------------------------------------------*/

class TColorXTerm
{
public:

    constexpr TColorXTerm() noexcept;
    constexpr TColorXTerm(uint8_t idx) noexcept;
    constexpr operator uint8_t() const noexcept;

private:

    uint8_t _idx;
};

constexpr TColorXTerm::TColorXTerm() noexcept :
    TColorXTerm(0)
{
}

constexpr TColorXTerm::TColorXTerm(uint8_t idx) noexcept :
    _idx(idx)
{
}

constexpr TColorXTerm::operator uint8_t() const noexcept
{
    return _idx;
}

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* класс TColorDefault */
/*                                                                         */
/* Представляет цвет терминала по умолчанию. Эмулятор использует его, */
/* если атрибуты вывода, например жирность и цвет, */
/* не заданы: обычно белый для текста и чёрный для фона. */
/*                                                                         */
/*-------------------------------------------------------------------------*/

class TColorDefault
{
public:

    TColorDefault() = default;
};

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* Функции преобразования цветов. */
/*                                                                         */
/* Преобразуют некоторые из описанных выше типов цвета. */
/*                                                                         */
/* Преобразование индексов XTerm 0–15 в RGB не предоставляется, поскольку */
/* их отображение зависит от эмулятора терминала. */
/* обычно позволяющего настраивать эти цвета с помощью цветовых */
/*  schemes.                                                               */
/*                                                                         */
/* Обратное преобразование RGB в индексы XTerm 0–15 предоставляется, */
/* но это приближение на основе цветового тона. */
/*                                                                         */
/* Для индексов XTerm 16–255 предполагается, что они */
/* соответствуют тем же значениям RGB, что и в XTerm: */
/*  https://invisible-island.net/xterm/xterm.faq.html#what_256colors       */
/*  https://jonasjacek.github.io/colors/                                   */
/*                                                                         */
/*-------------------------------------------------------------------------*/

class TColorConversion
{
public:

    static constexpr TColorXTerm BIOStoXTerm16(TColorBIOS) noexcept;
    static constexpr TColorBIOS XTerm16toBIOS(TColorXTerm) noexcept; // Индексы XTerm 0–15.
    static TColorXTerm XTerm256toXTerm16(TColorXTerm) noexcept; // Индексы XTerm 16–255 → 0–15.
    static TColorRGB XTerm256toRGB(TColorXTerm) noexcept; // Индексы XTerm 16–255.
    static TColorXTerm RGBtoXTerm16(TColorRGB) noexcept; // Индексы XTerm 0–15.
    static TColorXTerm RGBtoXTerm256(TColorRGB) noexcept; // Индексы XTerm 16–255.

private:

    friend class TColorConversionInit;

    template <class T, size_t N>
    struct LUT { T entries[N]; };

    static const LUT<TColorXTerm, 256> XTerm256toXTerm16LUT;
    static const LUT<TColorRGB, 256> XTerm256toRGBLUT;
};

constexpr TColorXTerm TColorConversion::BIOStoXTerm16(TColorBIOS c) noexcept
{
    // Меняем местами биты красного и синего.
    auto aux = c.getBlue();
    c.setBlue(c.getRed());
    c.setRed(aux);
    return TColorXTerm(c);
}

constexpr TColorBIOS TColorConversion::XTerm16toBIOS(TColorXTerm idx) noexcept
{
    return (uint8_t) BIOStoXTerm16((uint8_t) idx);
}

inline TColorXTerm TColorConversion::XTerm256toXTerm16(TColorXTerm idx) noexcept
{
    return XTerm256toXTerm16LUT.entries[idx];
}

inline TColorRGB TColorConversion::XTerm256toRGB(TColorXTerm idx) noexcept
{
    return XTerm256toRGBLUT.entries[idx];
}

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* класс TColor */
/*                                                                         */
/* Представляет цвет любого поддерживаемого типа. Допускает */
/* инициализацию объектом TColorBIOS, TColorRGB, TColorXTerm или */
/* TColorDefault. */
/*                                                                         */
/* Также допускается инициализация цвета BIOS символьным литералом; */
/* цвета RGB — целочисленным литералом; цвета терминала по умолчанию — */
/* инициализацией по умолчанию или инициализацией значением: */
/*      TColor bios = '\xF',                                               */
/*             rgb  = 0x7F00BB,                                            */
/* def; // либо 'def = {}'. */
/*                                                                         */
/* TColor тривиально копируем: для копирования объектов можно использовать memcpy. */
/*                                                                         */
/*-------------------------------------------------------------------------*/

class TColor
{
public:

    constexpr TColor() noexcept; // Цвет по умолчанию.

    // Конструкторы для литералов.

    constexpr TColor(char bios) noexcept; // Например, {'\xF'}.
    constexpr TColor(int rgb) noexcept; // Например, {0x7F00BB}.

    // Конструкторы для конкретных типов цвета.

    constexpr TColor(TColorBIOS bios) noexcept;
    constexpr TColor(TColorRGB rgb) noexcept;
    constexpr TColor(TColorXTerm xterm) noexcept;
    constexpr TColor(TColorDefault) noexcept;

    // Методы получения типа цвета.

    constexpr bool isDefault() const noexcept;
    constexpr bool isBIOS() const noexcept;
    constexpr bool isRGB() const noexcept;
    constexpr bool isXTerm() const noexcept;

    // Методы получения значения цвета не выполняют преобразований: сначала
    // проверьте тип.

    constexpr TColorBIOS asBIOS() const noexcept;
    constexpr TColorRGB asRGB() const noexcept;
    constexpr TColorXTerm asXTerm() const noexcept;

    // Квантование в TColorBIOS.

    TColorBIOS toBIOS(bool isForeground) const noexcept;

    constexpr bool operator==(TColor other) const noexcept;
    constexpr bool operator!=(TColor other) const noexcept;

private:

    // С бита 0: цвет, 24 бита. С бита 24: тип, 8 бит, из которых
    // используются только два.
    uint32_t _data;

    friend struct TColorAttr;

    enum : uint8_t
    {
        ctDefault = 0x0,
        ctBIOS    = 0x1,
        ctRGB     = 0x2,
        ctXTerm   = 0x3,
    };

    constexpr uint8_t type() const noexcept;
};

constexpr TColor::TColor() noexcept :
    _data(0)
{
}

constexpr TColor::TColor(char bios) noexcept :
    _data(uchar(bios & 0xF) | (ctBIOS << 24))
{
}

constexpr TColor::TColor(int rgb) noexcept :
    _data((rgb & 0xFFFFFF) | (ctRGB << 24))
{
}

constexpr TColor::TColor(TColorBIOS bios) noexcept :
    TColor((char) bios)
{
}

constexpr TColor::TColor(TColorRGB rgb) noexcept :
    TColor((int) rgb)
{
}

constexpr TColor::TColor(TColorXTerm xterm) noexcept :
    _data(xterm | (ctXTerm << 24))
{
}

constexpr TColor::TColor(TColorDefault) noexcept :
    TColor()
{
}

constexpr bool TColor::isDefault() const noexcept
{
    return type() == ctDefault;
}

constexpr bool TColor::isBIOS() const noexcept
{
    return type() == ctBIOS;
}

constexpr bool TColor::isRGB() const noexcept
{
    return type() == ctRGB;
}

constexpr bool TColor::isXTerm() const noexcept
{
    return type() == ctXTerm;
}

constexpr TColorBIOS TColor::asBIOS() const noexcept
{
    return uint8_t(_data);
}

constexpr TColorRGB TColor::asRGB() const noexcept
{
    return _data;
}

constexpr TColorXTerm TColor::asXTerm() const noexcept
{
    return uint8_t(_data);
}

constexpr bool TColor::operator==(TColor other) const noexcept
{
    return _data == other._data;
}

constexpr bool TColor::operator!=(TColor other) const noexcept
{
    return _data != other._data;
}

constexpr uint8_t TColor::type() const noexcept
{
    return _data >> 24;
}

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* структура TColorAttr */
/*                                                                         */
/* Представляет цвета и стиль текста, выводимого на экран. */
/*                                                                         */
/* TColorAttr совместим с memset и memcpy, */
/* поэтому имеет тривиальные конструктор и копирование. По этой */
/* причине переменные данного типа без явной инициализации */
/* содержат неинициализированные данные. Будьте внимательны! */
/*                                                                         */
/* TColorAttr, обнулённый через memset или инициализацию значением, */
/* имеет цвета текста и фона по умолчанию и не содержит */
/* флагов стиля. Поэтому обнулённый TColorAttr допустим */
/* и даёт видимый текст. */
/*                                                                         */
/* Примеры: */
/*                                                                         */
/* // Текст: BIOS 0x7 */
/* // Фон: RGB 0x7F00BB */
/* // Стиль: жирный, курсив */
/*      TColorAttr a = {'\x07', 0x7F00BB, slBold | slItalic};              */
/*                                                                         */
/* // Текст: по умолчанию */
/* // Фон: BIOS 0xF */
/* // Стиль: обычный */
/*      TColorAttr b = {{}, '\xF'};                                        */
/*                                                                         */
/* // Текст: по умолчанию */
/* // Фон: по умолчанию */
/* // Стиль: обычный */
/*      TColorAttr c {};                                                   */
/*                                                                         */
/* // Неинициализированный TColorAttr. Будьте внимательны! */
/*      TColorAttr d;                                                      */
/*                                                                         */
/* Для обратной совместимости TColorAttr также можно инициализировать */
/* цветовым атрибутом BIOS: */
/*                                                                         */
/* // Текст: BIOS 0xD */
/* // Фон: BIOS 0x3 */
/* // Стиль: обычный */
/*      TColorAttr e = 0x3D;                                               */
/*                                                                         */
/*-------------------------------------------------------------------------*/

const ushort

// Маски стилей TColorAttr.

    slBold         = 0x001,
    slItalic       = 0x002,
    slUnderline    = 0x004,
    slBlink        = 0x008,
    slReverse      = 0x010, // Предпочтительно использовать reversed().
    slStrike       = 0x020,

// Закрытые маски для внутреннего использования в TV.

    slWindowShadow = 0x200;

class TAttrPair;

struct TColorAttr
{
    TColorAttr() = default; // Внимание: конструктор тривиальный и не инициализирует поля.

    constexpr TColorAttr(int bios) noexcept;
    constexpr TColorAttr(TColor fg, TColor bg, ushort style = 0) noexcept;

    constexpr TColorAttr(const TAttrPair &attrs) noexcept;

    constexpr TColor getForeground() const noexcept;
    constexpr void setForeground(TColor fg) noexcept;
    constexpr TColor getBackground() const noexcept;
    constexpr void setBackground(TColor bg) noexcept;
    constexpr ushort getStyle() const noexcept;
    constexpr void setStyle(ushort aStyle) noexcept;

    constexpr TColorAttr reversed() const noexcept;

    // Преобразует в цветовой атрибут BIOS, при необходимости с квантованием.
    // Флаги стиля игнорируются.

    uchar toBIOS() const noexcept;

    // Функции обратной совместимости:

    // Возвращает соответствующий цветовой атрибут BIOS, если оба цвета имеют
    // тип BIOS и флаги стиля отсутствуют. Иначе возвращает фиксированный
    // цвет — белый на пурпурном — как признак недопустимого приведения.
    constexpr operator uchar() const noexcept;
    // Используется для составления пар атрибутов в старом коде.
    constexpr TAttrPair operator<<(int shift) const noexcept;

    constexpr bool operator==(TColorAttr other) const noexcept;
    constexpr bool operator!=(TColorAttr other) const noexcept;
    // Явно перегружаем сравнение с int, чтобы избежать неоднозначности.
    constexpr bool operator==(int bios) const noexcept;
    constexpr bool operator!=(int bios) const noexcept;

private:

    // Битовое поле реализовано вручную, чтобы компилятор мог оптимизировать
    // копирование и сравнение, сохранив constexpr. С бита 0: цвет текста, 27
    // бит; с бита 27: цвет фона, 27 бит; с бита 54: стиль, 10 бит.
    //
    //
    uint64_t _data;

    enum : uint64_t
    {
        fgMask = ((1ULL << 27) - 1),
        bgMask = ((1ULL << 27) - 1),
        styleMask = ((1ULL << 10) - 1),
    };
};

constexpr TColorAttr::TColorAttr(int bios) noexcept :
    TColorAttr(
        TColor(char(bios)),
        TColor(char(bios >> 4))
    )
{
}

constexpr TColorAttr::TColorAttr(TColor fg, TColor bg, ushort style) noexcept :
    _data(
        (fg._data & fgMask)
        | ((bg._data & bgMask) << 27)
        | (uint64_t(style) << 54)
    )
{
}

constexpr TColor TColorAttr::getForeground() const noexcept
{
    TColor color {};
    color._data = _data & fgMask;
    return color;
}

constexpr void TColorAttr::setForeground(TColor fg) noexcept
{
    _data = (_data & ~fgMask) | (fg._data & fgMask);
}

constexpr TColor TColorAttr::getBackground() const noexcept
{
    TColor color {};
    color._data = (_data >> 27) & bgMask;
    return color;
}

constexpr void TColorAttr::setBackground(TColor bg) noexcept
{
    _data = (_data & ~(bgMask << 27)) | ((bg._data & bgMask) << 27);
}

constexpr ushort TColorAttr::getStyle() const noexcept
{
    return _data >> 54;
}

constexpr void TColorAttr::setStyle(ushort aStyle) noexcept
{
    _data = (_data & ~(styleMask << 54)) | (uint64_t(aStyle) << 54);
}

constexpr TColorAttr TColorAttr::reversed() const noexcept
{
    auto fg = getForeground(),
         bg = getBackground();
    // Эмуляторы терминала могут по-разному отображать slReverse. Поэтому
    // лучше вручную менять цвета местами, если ни один из них не является
    // цветом по умолчанию.
    if (fg.isDefault() || bg.isDefault())
        return TColorAttr(fg, bg, getStyle() ^ slReverse);
    else
        return TColorAttr(bg, fg, getStyle());
}

constexpr TColorAttr::operator uchar() const noexcept
{
    // 'this' должен содержать атрибут BIOS, иначе возвращается фиксированный
    // цвет. Важно не получать '\x0', кроме случая атрибута BIOS '\x0': это
    // сохраняет работу старого кода, сравнивающего TColorAttr с нулём.
    //
    auto fg = getForeground(),
         bg = getBackground();
    if (fg.isBIOS() && bg.isBIOS() && getStyle() == 0)
        return fg.asBIOS() | (bg.asBIOS() << 4);
    return 0x5F;
}

constexpr bool TColorAttr::operator==(TColorAttr other) const noexcept
{
    return _data == other._data;
}

constexpr bool TColorAttr::operator!=(TColorAttr other) const noexcept
{
    return !(*this == other);
}

constexpr bool TColorAttr::operator==(int bios) const noexcept
{
    return *this == TColorAttr((uchar) bios);
}

constexpr bool TColorAttr::operator!=(int bios) const noexcept
{
    return !(*this == bios);
}

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* класс TAttrPair */
/*                                                                         */
/* Представляет пару цветовых атрибутов. */
/*                                                                         */
/* Пример: */
/*                                                                         */
/*      TColorAttr cNormal = {0x234983, 0x267232};                         */
/*      TColorAttr cHigh = {0x309283, 0x127844};                           */
/*      TAttrPair attrs = {cNormal, cHigh};                                */
/*      assert(attrs[0] == cNormal);                                       */
/*      assert(attrs[1] == cHigh);                                         */
/*      TDrawBuffer b;                                                     */
/*      b.moveCStr(0, "Normal text, ~Highlighted text~", attrs);           */
/*                                                                         */
/*-------------------------------------------------------------------------*/


class TAttrPair
{
public:

    constexpr TAttrPair(TColorAttr low = 0x00, TColorAttr high = 0x00) noexcept;
    constexpr TAttrPair(int bios) noexcept;

    // TAttrPair можно использовать как массив из двух элементов: [0] — младший, [1] —
    // старший. Вместо operator[] используются операторы преобразования в ссылку на
    // массив: они позволяют компилятору проверять границы по длине массива.
    using ArrayRef = TColorAttr (&)[2];
    constexpr operator ArrayRef() noexcept;
    using ConstArrayRef = const TColorAttr (&)[2];
    constexpr operator ConstArrayRef() const noexcept;

    // Функции обратной совместимости:

    // Возвращает соответствующую пару атрибутов BIOS. Работает правильно,
    // только если оба атрибута действительно имеют тип BIOS; см.
    // TColorAttr::operator uchar().
    constexpr operator ushort() const noexcept;
    // Используется для составления пар атрибутов в старом коде.
    constexpr TAttrPair operator>>(int shift) const noexcept;
    constexpr TAttrPair& operator|=(TColorAttr attr) noexcept;

private:

    TColorAttr _attrs[2];
};

constexpr TAttrPair::TAttrPair(TColorAttr low, TColorAttr high) noexcept :
    _attrs {low, high}
{
}

constexpr TAttrPair::TAttrPair(int bios) noexcept :
    _attrs {uchar(bios & 0xFF), uchar(bios >> 8)}
{
}

constexpr TAttrPair::operator TAttrPair::ArrayRef() noexcept
{
    return _attrs;
}

constexpr TAttrPair::operator TAttrPair::ConstArrayRef() const noexcept
{
    return _attrs;
}

constexpr TAttrPair::operator ushort() const noexcept
{
    return uchar(_attrs[0]) | (uchar(_attrs[1]) << 8);
}

constexpr TAttrPair TAttrPair::operator>>(int shift) const noexcept
{
    // В старом коде '>> 8' может извлекать старший атрибут пары.
    if (shift == 8)
        return {_attrs[1]};
    return ushort(*this) >> shift;
}

constexpr TAttrPair& TAttrPair::operator|=(TColorAttr attr) noexcept
{
    // В старом коде '|=' может задавать младший атрибут пары, но только если
    // тот эквивалентен атрибуту BIOS 0x00. В остальных случаях это обычная
    // арифметическая операция.
    if (_attrs[0] == 0)
        _attrs[0] = attr;
    else
        _attrs[0] = uchar(_attrs[0]) | uchar(attr);
    return *this;
}

// Ещё не определённые методы TColorAttr.

constexpr TColorAttr::TColorAttr(const TAttrPair &attrs) noexcept :
    TColorAttr(attrs[0])
{
}

constexpr TAttrPair TColorAttr::operator<<(int shift) const noexcept
{
    // В старом коде '<< 8' может создавать пару из одного атрибута.
    if (shift == 8)
        return {uchar(0), *this};
    return uchar(*this) << shift;
}

#endif // __BORLANDC__

#endif // TVISION_COLORS_H
