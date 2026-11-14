#ifndef TVISION_STRINGS_H
#define TVISION_STRINGS_H

#include <internal/constarr.h>

#include <stdint.h>

namespace tvision
{

template<class Int>
inline constexpr Int string_as_int(TStringView s) noexcept
// ВНИМАНИЕ: переинтерпретация результата как массива байтов зависит от порядка байтов.
{
    Int res = 0;
    for (size_t i = 0; i < min(s.size(), sizeof(res)); ++i)
        res |= uint64_t(uint8_t(s[i])) << 8*i;
    return res;
}

char *fast_utoa(uint32_t value, char *buffer) noexcept;

struct alignas(4) btoa_lut_elem_t
{
    char chars[3];
    uint8_t digits;
};

using btoa_lut_t = constarray<btoa_lut_elem_t, 256>;

inline char *fast_btoa(uint8_t value, char *buffer) noexcept
// Предусловие: ёмкость buffer — не меньше четырёх байт.
{
    extern const btoa_lut_t btoa_lut;
    // Оптимизация: читаем и записываем элемент таблицы преобразования
    // целиком, сокращая обращения к памяти. В buffer допустимо записать
    // больше байтов, чем число цифр.
    memcpy(buffer, &btoa_lut[value], 4);
    return buffer + (uint8_t) buffer[3];
    static_assert(sizeof(btoa_lut_elem_t) == 4, "");
}

} // пространство имён tvision

#endif // TVISION_STRINGS_H
