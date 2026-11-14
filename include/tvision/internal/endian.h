#ifndef TVISION_ENDIAN_H
#define TVISION_ENDIAN_H

#include <stdint.h>

namespace tvision
{

// Оптимизация: методы reverseBytes написаны так, чтобы компилятор,
// скорее всего, заменил их инструкцией BSWAP или её аналогом.

inline void reverseBytes(uint16_t &val)
{
    val = (val << 8) | (val >> 8);
}

inline void reverseBytes(uint32_t &val)
{
    val = ((val << 8) & 0xFF00FF00U) | ((val >> 8) & 0x00FF00FF);
    val = (val << 16) | (val >> 16);
}

inline void reverseBytes(uint64_t &val)
{
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
    val = (val << 32) | (val >> 32);
}

} // пространство имён tvision

#endif // TVISION_ENDIAN_H
