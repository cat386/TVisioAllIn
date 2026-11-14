#ifndef TVISION_BASE64_H
#define TVISION_BASE64_H

#include <tvision/tv.h>
#include <string>

namespace tvision
{

// Возвращает область output с закодированными данными. Предусловие:
// ёмкость output не меньше 4/3 от input.size().
TStringView encodeBase64(TStringView input, char *output) noexcept;

// Возвращает область output с декодированными данными. Предусловие:
// ёмкость output не меньше 3/4 от input.size().
TStringView decodeBase64(TStringView input, char *output) noexcept;

inline std::string encodeBase64(TStringView input)
{
    std::string result((input.size() * 4)/3 + 4, '\0');
    auto encoded = encodeBase64(input, &result[0]);
    result.resize(encoded.size());
    return result;
}

inline std::string decodeBase64(TStringView input)
{
    std::string result((input.size() * 3)/4 + 3, '\0');
    auto encoded = decodeBase64(input, &result[0]);
    result.resize(encoded.size());
    return result;
}

} // пространство имён tvision

#endif // TVISION_BASE64_H
