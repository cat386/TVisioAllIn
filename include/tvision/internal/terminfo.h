#ifndef TVISION_TERMINFO_H
#define TVISION_TERMINFO_H

#include <tvision/tv.h>

#ifdef _TV_UNIX

#include <string>
#include <vector>

namespace tvision
{

// TermInfo — автономный читатель базы terminfo, описывающей возможности
// типов терминалов. Заменяет библиотеки Ncurses/Tinfo. Поддерживает
// старый двоичный формат с 16-битными числами и расширенный с 32-битными
// числами, используемый, например, xterm-direct, а также
// пользовательские возможности вроде kDC5 и kUP3. Если тип терминала
// отсутствует в базе, используется встроенное описание; см.
// TermInfo::builtin.
//
//
//

class TermInfo
{
public:

    struct StrCap
    {
        std::string name;
        std::string value;
    };

    // Имя типа терминала, переданное load().
    std::string termName;
    // Путь к файлу базы, откуда прочитана запись; пустой для встроенного описания.
    std::string sourcePath;

    std::vector<std::pair<std::string, bool>> bools;
    std::vector<std::pair<std::string, int>> nums;
    std::vector<StrCap> strs;

    // Возвращает описание типа терминала termName. Результат кэшируется,
    // поэтому повторные вызовы недороги. Если тип отсутствует в базе,
    // getFromDatabase возвращает false и используется встроенное описание.
    //
    static const TermInfo &get(const char *termName) noexcept;

    // Загружает запись termName из базы terminfo. Возвращает false, если
    // запись не найдена или не удалось её разобрать.
    static bool loadFromDatabase(const char *termName, TermInfo &result) noexcept;
    // Разбирает скомпилированную запись terminfo.
    static bool parse(const char *data, size_t size, TermInfo &result) noexcept;
    // Заполняет result встроенным описанием, похожим на termName.
    static void builtin(const char *termName, TermInfo &result) noexcept;

    bool fromDatabase() const noexcept { return !sourcePath.empty(); }

    // Методы доступа. Отсутствующие и отменённые возможности обрабатываются одинаково.
    bool getBool(const char *name) const noexcept;
    int getNum(const char *name) const noexcept; // Возвращает -1 при отсутствии.
    const char *getStr(const char *name) const noexcept; // Возвращает nullptr при отсутствии.

    // Подставляет параметры в строку возможности, например cup или csr, как
    // функция tparm() библиотеки terminfo. Поддерживает до девяти числовых
    // параметров.
    static std::string tparm(const char *str, const int *params, int paramCount) noexcept;

    // Проверяет, относится ли name к стандартным, а не пользовательским
    // строковым возможностям.
    static bool isStandardStrCap(const char *name) noexcept;

    // Число цветов терминала по правилам Ncurses: если терминал не позволяет
    // менять цвета, возвращается 0.
    int getColorCount() const noexcept;
};

} // пространство имён tvision

#endif // _TV_UNIX

#endif // TVISION_TERMINFO_H
