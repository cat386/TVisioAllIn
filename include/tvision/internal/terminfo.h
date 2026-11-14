#ifndef TVISION_TERMINFO_H
#define TVISION_TERMINFO_H

#include <tvision/tv.h>

#ifdef _TV_UNIX

#include <string>
#include <vector>

namespace tvision
{

// TermInfo is a self-contained reader for the terminfo database, which
// describes the capabilities of terminal types. It replaces the use of the
// Ncurses/Tinfo libraries.
//
// Both the legacy binary format (16-bit numbers) and the extended format
// (32-bit numbers, used for entries such as 'xterm-direct') are supported,
// as are user-defined (extended) capabilities such as 'kDC5' or 'kUP3'.
//
// When the terminal type cannot be found in the database, a built-in
// description is used (see 'TermInfo::builtin').

class TermInfo
{
public:

    struct StrCap
    {
        std::string name;
        std::string value;
    };

    // The name of the terminal type, as given to 'load()'.
    std::string termName;
    // Path of the database file the entry was read from (empty if built-in).
    std::string sourcePath;

    std::vector<std::pair<std::string, bool>> bools;
    std::vector<std::pair<std::string, int>> nums;
    std::vector<StrCap> strs;

    // Returns the description of the terminal type 'termName'. The result
    // is cached, so it is cheap to call this repeatedly.
    // If the terminal type is not in the database, 'getFromDatabase' returns
    // false and the result is a built-in description.
    static const TermInfo &get(const char *termName) noexcept;

    // Loads the entry for 'termName' from the terminfo database.
    // Returns false if it cannot be found or parsed.
    static bool loadFromDatabase(const char *termName, TermInfo &result) noexcept;
    // Parses a compiled terminfo entry.
    static bool parse(const char *data, size_t size, TermInfo &result) noexcept;
    // Fills 'result' with a built-in description resembling 'termName'.
    static void builtin(const char *termName, TermInfo &result) noexcept;

    bool fromDatabase() const noexcept { return !sourcePath.empty(); }

    // Accessors. Absent and cancelled capabilities are treated the same way.
    bool getBool(const char *name) const noexcept;
    int getNum(const char *name) const noexcept; // Returns -1 if absent.
    const char *getStr(const char *name) const noexcept; // Returns nullptr if absent.

    // Instantiates a parameterized capability string (such as 'cup' or
    // 'csr'), like the 'tparm()' function of the terminfo library. Up to 9
    // numeric parameters are supported.
    static std::string tparm(const char *str, const int *params, int paramCount) noexcept;

    // Whether 'name' is one of the standard (non user-defined) string
    // capabilities.
    static bool isStandardStrCap(const char *name) noexcept;

    // Number of colors supported by the terminal, following Ncurses'
    // semantics: if the terminal cannot change colors, this is 0.
    int getColorCount() const noexcept;
};

} // namespace tvision

#endif // _TV_UNIX

#endif // TVISION_TERMINFO_H
