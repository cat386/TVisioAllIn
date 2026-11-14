#define Uses_TColorAttr
#include <tvision/tv.h>

// Алгоритм преобразования RGB в XTerm16. XTerm16 — нерегулярная 4-битная
// палитра RGBI, что часто игнорируется в других решениях. Решения на
// основе CIEDE2000 тоже представляются неубедительными. Здесь
// промежуточно преобразуем RGB в HSL. Затем определяем, нужно ли
// приближать цвет оттенком серого. Для серого выбираем один из четырёх
// уровней; для цветного — тёмный либо яркий вариант по компоненту L.
// Окончательный цвет выбирается по H. Результат зрительно ближе к
// исходному, чем в других известных автору решениях. Алгоритм пригоден
// для вычисления в реальном времени: использует целочисленную арифметику
// и не более одного деления.
//
//
//
//
//
//
//
//
//

struct HCL
{
    uint8_t h; // [0..HUE_MAX)
    uint8_t c; // [0..255]
    uint8_t l; // [0..255]
};

constexpr uint8_t HUE_PRECISION = 32;
constexpr uint8_t HUE_MAX = 6*HUE_PRECISION;

static constexpr
HCL RGBtoHCL(uint8_t R, uint8_t G, uint8_t B) noexcept
{
    uint8_t Xmin = min(min(R, G), B);
    uint8_t Xmax = max(max(R, G), B);
    uint8_t V = Xmax;
    uint8_t L = uint16_t(Xmax + Xmin)/2;
    uint8_t C = Xmax - Xmin;
    int16_t H = 0;
    if (C)
    {
        if (V == R)
            H = int16_t(HUE_PRECISION*(G - B))/C;
        else if (V == G)
            H = int16_t(HUE_PRECISION*(B - R))/C + 2*HUE_PRECISION;
        else if (V == B)
            H = int16_t(HUE_PRECISION*(R - G))/C + 4*HUE_PRECISION;

        if (H < 0)
            H += HUE_MAX;
        else if (H >= HUE_MAX)
            H -= HUE_MAX;
    }

    return {(uint8_t) H, C, L};
}

static constexpr uint8_t u8(double d) noexcept
{
    return uint8_t(d*255);
}

static constexpr
TColorXTerm RGBtoXTerm16Impl(uint8_t r, uint8_t g, uint8_t b) noexcept
{
    HCL c = RGBtoHCL(r, g, b);

    if (c.c >= 12) // Цветной вариант, если Chroma >= 12.
    {
        constexpr uint8_t hueToXTerm[6] = {1, 3, 2, 6, 4, 5};
        uint8_t hue = uint8_t(c.h < HUE_MAX - HUE_PRECISION/2 ?
                                  c.h + HUE_PRECISION/2
                                : c.h - (HUE_MAX - HUE_PRECISION/2)
                             )/HUE_PRECISION;
        if (c.l < u8(0.5))
            return hueToXTerm[hue];
        if (c.l < u8(0.925))
            return hueToXTerm[hue] + 8; // Яркие цвета: {9, 11, 10, 14, 12, 13}.
        return 15;
    }
    else
    {
        if (c.l < u8(0.25))
            return 0;
        if (c.l < u8(0.625))
            return 8;
        if (c.l < u8(0.875))
            return 7;
        return 15;
    }
}

class TColorConversionInit
{
public:

    static constexpr TColorConversion::LUT<TColorXTerm, 256> initXTerm256toXTerm16LUT() noexcept;
    static constexpr TColorConversion::LUT<TColorRGB, 256> initXTerm256toRGBLUT() noexcept;
};

constexpr TColorConversion::LUT<TColorXTerm, 256> TColorConversionInit::initXTerm256toXTerm16LUT() noexcept
{
    TColorConversion::LUT<TColorXTerm, 256> lut {};
    for (uint8_t i = 0; i < 16; ++i)
        lut.entries[i] = i;
    for (uint8_t i = 0; i < 6; ++i)
    {
        uint8_t R = i ? 55 + i*40 : 0;
        for (uint8_t j = 0; j < 6; ++j)
        {
            uint8_t G = j ? 55 + j*40 : 0;
            for (uint8_t k = 0; k < 6; ++k)
            {
                uint8_t B = k ? 55 + k*40 : 0;
                TColorXTerm idx16 = RGBtoXTerm16Impl(R, G, B);
                lut.entries[16 + (i*6 + j)*6 + k] = idx16;
            }
        }
    }
    for (uint8_t i = 0; i < 24; ++i)
    {
        uint8_t L = i * 10 + 8;
        TColorXTerm idx16 = RGBtoXTerm16Impl(L, L, L);
        lut.entries[232 + i] = idx16;
    }
    return lut;
}

constexpr TColorConversion::LUT<TColorRGB, 256> TColorConversionInit::initXTerm256toRGBLUT() noexcept
{
    TColorConversion::LUT<TColorRGB, 256> lut {};
    // Только индексы 16..255.
    for (uint8_t i = 0; i < 6; ++i)
    {
        uint8_t R = i ? 55 + i*40 : 0;
        for (uint8_t j = 0; j < 6; ++j)
        {
            uint8_t G = j ? 55 + j*40 : 0;
            for (uint8_t k = 0; k < 6; ++k)
            {
                uint8_t B = k ? 55 + k*40 : 0;
                lut.entries[16 + (i*6 + j)*6 + k] = {R, G, B};
            }
        }
    }
    for (uint8_t i = 0; i < 24; ++i)
    {
        uint8_t L = i * 10 + 8;
        lut.entries[232 + i] = {L, L, L};
    }
    return lut;
}

constexpr TColorConversion::LUT<TColorXTerm, 256> TColorConversion::XTerm256toXTerm16LUT =
    TColorConversionInit::initXTerm256toXTerm16LUT();
constexpr TColorConversion::LUT<TColorRGB, 256> TColorConversion::XTerm256toRGBLUT =
    TColorConversionInit::initXTerm256toRGBLUT();

TColorXTerm TColorConversion::RGBtoXTerm16(TColorRGB c) noexcept
{
    return RGBtoXTerm16Impl(c.getRed(), c.getGreen(), c.getBlue());
}

TColorXTerm TColorConversion::RGBtoXTerm256(TColorRGB c) noexcept
{
    // Палитра xterm-256color состоит из 16 цветов xterm-16color в диапазоне
    // [0..15], 216 цветов куба 6×6×6 в [16..231] и 24 оттенков серого в
    // [232..255]. Эта функция не возвращает индексы [0..15]; для них
    // используйте RGBtoXTerm16. В кубе недостаточно тёмных цветов: уровни
    // каналов [0, 1, 2, 3, 4, 5] соответствуют значениям [0, 95, 135, 175,
    // 215, 255]. Кроме нуля, расстояние между уровнями равно 40. Значения
    // меньше 95 - 40/2 = 75 пришлось бы округлять в 0. Для частичной
    // компенсации разрешаем отображать [55..74] в 1. Кроме того, используем
    // оттенки серого, если куб округлил бы цвет до чисто чёрного: так
    // сохраняются иначе теряемые детали.
    //
    //
    //
    //
    //
    //
    //
    //
    //
    auto cnvColor = [] (TColorRGB c)
    {
        auto scale = [] (uchar c)
        {
            c += 20 & -(c < 75); // Компенсируем недостаточное представление тёмных цветов.
            return uchar(max<uchar>(c, 35) - 35)/40;
        };
        uchar r = scale(c.getRed()),
              g = scale(c.getGreen()),
              b = scale(c.getBlue());
        return 16 + uchar(r*uchar(6) + g)*uchar(6) + b;
    };
    auto cnvGray = [] (uchar l)
    {
        if (l < 8 - 5)
            return 16; // Полностью чёрный.
        if (l >= 238 + 5)
            return 231; // Полностью белый. Теперь L находится в диапазоне [3..242] и должен
        // соответствовать одному из 24 оттенков серого.
        //
        return 232 + uchar(max<uchar>(l, 3) - 3)/uchar(10); // [232..255]
    };

    uchar idx = cnvColor(c);
    if (c != XTerm256toRGB(idx))
    {
        uchar Xmin = min(min(c.getRed(), c.getGreen()), c.getBlue()),
              Xmax = max(max(c.getRed(), c.getGreen()), c.getBlue());
        uchar C = Xmax - Xmin; // Цветность в модели HSL/HSV. Для тёмных или малонасыщенных цветов, не
        // представимых точно кубом 6×6×6, используем шкалу серого.
        //
        if (C < 12 || idx == 16) // Серый, если Chroma < 12 или цвет округлён до чёрного.
        {
            uchar L = ushort(Xmax + Xmin)/2; // Светлота, как в HSL.
            idx = cnvGray(L);
        }
    }
    return idx;
}

TColorBIOS TColor::toBIOS(bool isForeground) const noexcept
{
    switch (type())
    {
        case ctBIOS:
            return asBIOS();
        case ctRGB:
            return TColorConversion::XTerm16toBIOS(
                TColorConversion::RGBtoXTerm16(asRGB())
            );
        case ctXTerm:
        {
            uint8_t idx = asXTerm();
            if (idx >= 16)
                idx = TColorConversion::XTerm256toXTerm16(idx);
            return TColorConversion::XTerm16toBIOS(idx);
        }
        default:
            return isForeground ? 0x7 : 0x0;
    }
}

uchar TColorAttr::toBIOS() const noexcept
{
    return getForeground().toBIOS(true) | (getBackground().toBIOS(false) << 4);
}
