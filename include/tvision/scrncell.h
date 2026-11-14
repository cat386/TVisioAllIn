/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   SCRNCELL.H                                                            */
/*                                                                         */
/* Определяет структуры TScreenCharacter и TScreenCell. */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#ifndef TVISION_SCRNCELL_H
#define TVISION_SCRNCELL_H

#if defined( __BORLANDC__ )

// Структура повторяет расположение в памяти ячейки текстового экрана DOS.
// Собственные оптимизированные операторы преобразуют её в unsigned short и обратно.

struct TScreenCell
{
    char character;
    TColorAttr attribute;

    TScreenCell& operator=(const TScreenCell &other);
    TScreenCell& operator=(ushort value);
    operator ushort() const;
};

inline TScreenCell& TScreenCell::operator=(const TScreenCell &other)
{
    (ushort &) *this = (const ushort &) other;
    return *this;
}

inline TScreenCell& TScreenCell::operator=(ushort value)
{
    (ushort &) *this = value;
    return *this;
}

inline TScreenCell::operator ushort() const
{
    return (const ushort &) *this;
}

#else

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* структура TScreenCharacter */
/*                                                                         */
/* Представляет текст ячейки экрана. Обычно обращаться */
/* к нему напрямую не нужно. Для записи текста в ячейку */
/* используйте функции пространства имён TText. */
/*                                                                         */
/* TScreenCharacter хранит один из следующих вариантов: */
/* 1. Один байт ASCII или расширенной ASCII, занимающий один столбец. */
/* 2. До 15 байтов UTF-8 суммарной шириной один или два столбца; */
/* текст не может состоять только из символов нулевой ширины. */
/* 3. Специальное значение — маркер продолжения широкого символа. */
/*                                                                         */
/* Для полного отображения символа двойной ширины за его */
/* ячейкой должна следовать ячейка с маркером продолжения. Иначе, */
/* а также если перед маркером нет символа двойной */
/* ширины, считается, что широкий символ частично */
/* перекрыт. */
/*                                                                         */
/* TScreenCharacter совместим с memset и */
/* memcpy, поэтому его создание и копирование тривиальны. */
/* Из-за этого переменные данного типа без явной */
/* инициализации содержат неинициализированные данные. Будьте внимательны! */
/*                                                                         */
/* TScreenCharacter, обнулённый через memset или */
/* инициализацию значением, допустим и представляет текст пустой */
/* ячейки экрана. */
/*                                                                         */
/*-------------------------------------------------------------------------*/

struct TScreenCharacter
{
    TScreenCharacter() = default; // Внимание: конструктор тривиальный и не инициализирует поля.

    constexpr TScreenCharacter(char c) noexcept;

    constexpr void initWithChar(char c) noexcept;
    constexpr void initWithMultiByteChar(uint32_t mbc, bool wide = false) noexcept;
    constexpr void initWithMultiByteChar(TStringView mbc, bool wide = false) noexcept;
    constexpr void initAsWideCharTrail() noexcept;

    constexpr bool isWide() const noexcept;
    constexpr bool isWideCharTrail() const noexcept;
    constexpr void appendZeroWidthChar(TStringView mbc) noexcept;
    constexpr TStringView getText() const noexcept;

private:

    enum : uint8_t
    {
        fWide     = 0x1,
        fTrail    = 0x2,
        fOverflow = 0x4,
    };

    char _text[15];
    uint8_t
        // Всегда содержит хотя бы один символ, даже если это нулевой байт.
        _textLengthMinusOne : 4,
        _flags : 4;
};

constexpr TScreenCharacter::TScreenCharacter(char c) noexcept :
    _text {c},
    _textLengthMinusOne(0),
    _flags(0)
{
}

constexpr void TScreenCharacter::initWithChar(char c) noexcept
{
    *this = {};
    _text[0] = c;
}

constexpr void TScreenCharacter::initWithMultiByteChar(uint32_t mbc, bool wide) noexcept
// Предусловие: mbc — многобайтовый символ с переинтерпретированными битами.
{
    *this = {};
#if !defined( TV_BIG_ENDIAN )
    _text[0] = char(mbc);
    _text[1] = char(mbc >> 8);
    _text[2] = char(mbc >> 16);
    _text[3] = char(mbc >> 24);

    _textLengthMinusOne =
        (mbc > 0x000000FFU) +
        (mbc > 0x0000FFFFU) +
        (mbc > 0x00FFFFFFU);
#else
    _text[0] = char(mbc >> 24);
    _text[1] = char(mbc >> 16);
    _text[2] = char(mbc >> 8);
    _text[3] = char(mbc);

    _textLengthMinusOne =
        ((mbc & 0x00FFFFFFU) != 0) +
        ((mbc & 0x0000FFFFU) != 0) +
        ((mbc & 0x000000FFU) != 0);
#endif
    _flags = -int(wide) & fWide;
}

constexpr void TScreenCharacter::initWithMultiByteChar(TStringView mbc, bool wide) noexcept
{
    static_assert(sizeof(_text) >= maxCharSize, "");
    *this = {};
    if (0 < mbc.size() && mbc.size() <= maxCharSize)
    {
        _flags |= -int(wide) & fWide;
        _textLengthMinusOne = mbc.size() - 1;
        switch (_textLengthMinusOne)
        {
            case 3: _text[3] = mbc[3];
            case 2: _text[2] = mbc[2];
            case 1: _text[1] = mbc[1];
            case 0: _text[0] = mbc[0];
        }
    }
}

constexpr void TScreenCharacter::initAsWideCharTrail() noexcept
{
    *this = {};
    _flags = fTrail;
}

constexpr bool TScreenCharacter::isWide() const noexcept
{
    return _flags & fWide;
}

constexpr bool TScreenCharacter::isWideCharTrail() const noexcept
{
    return _flags & fTrail;
}

constexpr void TScreenCharacter::appendZeroWidthChar(TStringView mbc) noexcept
// Предусловие: _text содержит корректный текст UTF-8.
{
    if (_flags & fOverflow)
        return;
    size_t sz = _textLengthMinusOne + 1;
    if (mbc.size() <= sizeof(_text) - sz)
    {
        if (_text[0] == '\0')
            _text[0] = ' ';
        switch (mbc.size())
        {
            case 4: _text[sz + 3] = mbc[3];
            case 3: _text[sz + 2] = mbc[2];
            case 2: _text[sz + 1] = mbc[1];
            case 1: _text[sz] = mbc[0];
        }
        _textLengthMinusOne += mbc.size();
    }
    else
        _flags |= fOverflow;
}

constexpr TStringView TScreenCharacter::getText() const noexcept
// Предусловие: isWideCharTrail() == false.
{
    return {_text, size_t(_textLengthMinusOne + 1)};
}

/*-------------------------------------------------------------------------*/
/*                                                                         */
/* структура TScreenCell */
/*                                                                         */
/* Хранит текст и цветовые атрибуты ячейки экрана. Обычно */
/* не нужно обращаться к ним напрямую. Для записи */
/* текста в ячейку используйте функции пространства имён TText. */
/*  namespace.                                                             */
/*                                                                         */
/* TScreenCell совместим с memset и memcpy, */
/* поэтому имеет тривиальные конструктор и копирование. По этой */
/* причине переменные данного типа без явной инициализации */
/* содержат неинициализированные данные. Будьте внимательны! */
/*                                                                         */
/* а обнулённый TScreenCell через memset или */
/* инициализацию значением допустим и представляет пустую ячейку */
/* с цветами текста и фона по умолчанию, без стилей. */
/*                                                                         */
/*-------------------------------------------------------------------------*/

struct TScreenCell
{
    TScreenCharacter character;
    TColorAttr attribute;

    TScreenCell() = default; // Внимание: конструктор тривиальный и не инициализирует поля.
    constexpr TScreenCell(const TScreenCharacter &ch, TColorAttr attr) noexcept;
    constexpr TScreenCell(ushort bios) noexcept;

    bool operator==(const TScreenCell &other) const;
    bool operator!=(const TScreenCell &other) const;
};

constexpr TScreenCell::TScreenCell(const TScreenCharacter &ch, TColorAttr attr) noexcept :
    character(ch),
    attribute(attr)
{
}

constexpr TScreenCell::TScreenCell(ushort bios) noexcept :
    character(char(bios)),
    attribute(uchar(bios >> 8))
{
}

inline bool TScreenCell::operator==(const TScreenCell &other) const
{
    return memcmp(this, &other, sizeof(*this)) == 0;
}

inline bool TScreenCell::operator!=(const TScreenCell &other) const
{
    return !(*this == other);
}

#endif // __BORLANDC__

#endif // TVISION_SCRNCELL_H
