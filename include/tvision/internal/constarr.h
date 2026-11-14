#ifndef TVISION_CONSTARR_H
#define TVISION_CONSTARR_H

#include <stddef.h>

// До C++17 std::array не поддерживает constexpr. Поэтому используем
// собственный вариант без дополнительных затрат.

namespace tvision
{

template <class T, size_t N>
struct constarray
{
    T elems[N];

    constexpr T& operator[](size_t i) noexcept
    {
        return elems[i];
    }

    constexpr const T& operator[](size_t i) const noexcept
    {
        return elems[i];
    }

};

} // пространство имён tvision

#endif // TVISION_CONSTARR_H
