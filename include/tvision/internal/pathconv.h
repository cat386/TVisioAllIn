#ifndef TVISION_PATHCONV_H
#define TVISION_PATHCONV_H

#include <string>
#include <algorithm>
#include <string.h>

namespace tvision
{

inline bool isDriveLetter(char c) noexcept
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

// path_dos2unix заменяет '\' на '/' и удаляет букву диска.

inline void path_dos2unix(std::string &s, bool drive=true) noexcept {
    std::replace(s.begin(), s.end(), '\\', '/');
    if (drive && s.size() > 1 && s[1] == ':' && isDriveLetter(s[0]))
        s = s.substr(2);
}

inline void path_dos2unix(char *c, bool drive=true) noexcept {
    char *d = c;
    while ((d = strchr(d, '\\')))
        *d = '/';
    if (drive && *c && c[1] == ':' && isDriveLetter(*c))
        memmove(c, c+2, strlen(c)-1); // Также копируем завершающий нулевой символ.
}

// path_unix2dos заменяет '/' на '\'.

inline void path_unix2dos(std::string &s) noexcept {
    std::replace(s.begin(), s.end(), '/', '\\');
}

inline void path_unix2dos(char *c) noexcept {
    while ((c = strchr(c, '/')))
        *c = '\\';
}

} // пространство имён tvision

#endif // TVISION_PATHCONV_H
