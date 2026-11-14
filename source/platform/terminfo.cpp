#include <internal/terminfo.h>

#ifdef _TV_UNIX

#include <internal/getenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

namespace tvision
{

// Имена стандартных возможностей в порядке хранения в скомпилированном
// terminfo: порядок System V, используемый также Ncurses.

static const char *const boolNames[] =
{
    "bw", "am", "xsb", "xhp", "xenl", "eo", "gn", "hc", "km", "hs", "in",
    "da", "db", "mir", "msgr", "os", "eslok", "xt", "hz", "ul", "xon", "nxon",
    "mc5i", "chts", "nrrmc", "npc", "ndscr", "ccc", "bce", "hls", "xhpa",
    "crxm", "daisy", "xvpa", "sam", "cpix", "lpix", "OTbs", "OTns", "OTnc",
    "OTMT", "OTNL", "OTpt", "OTxr",
};

static const char *const numNames[] =
{
    "cols", "it", "lines", "lm", "xmc", "pb", "vt", "wsl", "nlab", "lh", "lw",
    "ma", "wnum", "colors", "pairs", "ncv", "bufsz", "spinv", "spinh",
    "maddr", "mjump", "mcs", "mls", "npins", "orc", "orl", "orhi", "orvi",
    "cps", "widcs", "btns", "bitwin", "bitype", "OTug", "OTdC", "OTdN",
    "OTdB", "OTdT", "OTkn",
};

static const char *const strNames[] =
{
    "cbt", "bel", "cr", "csr", "tbc", "clear", "el", "ed", "hpa", "cmdch",
    "cup", "cud1", "home", "civis", "cub1", "mrcup", "cnorm", "cuf1", "ll",
    "cuu1", "cvvis", "dch1", "dl1", "dsl", "hd", "smacs", "blink", "bold",
    "smcup", "smdc", "dim", "smir", "invis", "prot", "rev", "smso", "smul",
    "ech", "rmacs", "sgr0", "rmcup", "rmdc", "rmir", "rmso", "rmul", "flash",
    "ff", "fsl", "is1", "is2", "is3", "if", "ich1", "il1", "ip", "kbs",
    "ktbc", "kclr", "kctab", "kdch1", "kdl1", "kcud1", "krmir", "kel", "ked",
    "kf0", "kf1", "kf10", "kf2", "kf3", "kf4", "kf5", "kf6", "kf7", "kf8",
    "kf9", "khome", "kich1", "kil1", "kcub1", "kll", "knp", "kpp", "kcuf1",
    "kind", "kri", "khts", "kcuu1", "rmkx", "smkx", "lf0", "lf1", "lf10",
    "lf2", "lf3", "lf4", "lf5", "lf6", "lf7", "lf8", "lf9", "rmm", "smm",
    "nel", "pad", "dch", "dl", "cud", "ich", "indn", "il", "cub", "cuf",
    "rin", "cuu", "pfkey", "pfloc", "pfx", "mc0", "mc4", "mc5", "rep", "rs1",
    "rs2", "rs3", "rf", "rc", "vpa", "sc", "ind", "ri", "sgr", "hts", "wind",
    "ht", "tsl", "uc", "hu", "iprog", "ka1", "ka3", "kb2", "kc1", "kc3",
    "mc5p", "rmp", "acsc", "pln", "kcbt", "smxon", "rmxon", "smam", "rmam",
    "xonc", "xoffc", "enacs", "smln", "rmln", "kbeg", "kcan", "kclo", "kcmd",
    "kcpy", "kcrt", "kend", "kent", "kext", "kfnd", "khlp", "kmrk", "kmsg",
    "kmov", "knxt", "kopn", "kopt", "kprv", "kprt", "krdo", "kref", "krfr",
    "krpl", "krst", "kres", "ksav", "kspd", "kund", "kBEG", "kCAN", "kCMD",
    "kCPY", "kCRT", "kDC", "kDL", "kslt", "kEND", "kEOL", "kEXT", "kFND",
    "kHLP", "kHOM", "kIC", "kLFT", "kMSG", "kMOV", "kNXT", "kOPT", "kPRV",
    "kPRT", "kRDO", "kRPL", "kRIT", "kRES", "kSAV", "kSPD", "kUND", "rfi",
    "kf11", "kf12", "kf13", "kf14", "kf15", "kf16", "kf17", "kf18", "kf19",
    "kf20", "kf21", "kf22", "kf23", "kf24", "kf25", "kf26", "kf27", "kf28",
    "kf29", "kf30", "kf31", "kf32", "kf33", "kf34", "kf35", "kf36", "kf37",
    "kf38", "kf39", "kf40", "kf41", "kf42", "kf43", "kf44", "kf45", "kf46",
    "kf47", "kf48", "kf49", "kf50", "kf51", "kf52", "kf53", "kf54", "kf55",
    "kf56", "kf57", "kf58", "kf59", "kf60", "kf61", "kf62", "kf63", "el1",
    "mgc", "smgl", "smgr", "fln", "sclk", "dclk", "rmclk", "cwin", "wingo",
    "hup", "dial", "qdial", "tone", "pulse", "hook", "pause", "wait", "u0",
    "u1", "u2", "u3", "u4", "u5", "u6", "u7", "u8", "u9", "op", "oc", "initc",
    "initp", "scp", "setf", "setb", "cpi", "lpi", "chr", "cvr", "defc",
    "swidm", "sdrfq", "sitm", "slm", "smicm", "snlq", "snrmq", "sshm",
    "ssubm", "ssupm", "sum", "rwidm", "ritm", "rlm", "rmicm", "rshm", "rsubm",
    "rsupm", "rum", "mhpa", "mcud1", "mcub1", "mcuf1", "mvpa", "mcuu1",
    "porder", "mcud", "mcub", "mcuf", "mcuu", "scs", "smgb", "smgbp", "smglp",
    "smgrp", "smgt", "smgtp", "sbim", "scsd", "rbim", "rcsd", "subcs",
    "supcs", "docr", "zerom", "csnm", "kmous", "minfo", "reqmp", "getm",
    "setaf", "setab", "pfxl", "devt", "csin", "s0ds", "s1ds", "s2ds", "s3ds",
    "smglr", "smgtb", "birep", "binel", "bicr", "colornm", "defbi", "endbi",
    "setcolor", "slines", "dispc", "smpch", "rmpch", "smsc", "rmsc", "pctrm",
    "scesc", "scesa", "ehhlm", "elhlm", "elohlm", "erhlm", "ethlm", "evhlm",
    "sgr1", "slength", "OTi2", "OTrs", "OTnl", "OTbc", "OTko", "OTma", "OTG2",
    "OTG3", "OTG1", "OTG4", "OTGR", "OTGL", "OTGU", "OTGD", "OTGH", "OTGV",
    "OTGC", "meml", "memu", "box1",
};

static constexpr size_t boolNameCount = sizeof(boolNames)/sizeof(boolNames[0]);
static constexpr size_t numNameCount = sizeof(numNames)/sizeof(numNames[0]);
static constexpr size_t strNameCount = sizeof(strNames)/sizeof(strNames[0]);

static_assert(boolNameCount == 44, "");
static_assert(numNameCount == 39, "");
static_assert(strNameCount == 414, "");

// Чтение двоичного формата. Все целые — little-endian. Запись terminfo
// начинается шестью 16-битными числами: сигнатура, размер секции имён,
// число логических, числовых и строковых возможностей и размер таблицы
// строк. Сигнатура 0432 в восьмеричной системе означает 16-битные числа,
// 01036 — 32-битные. Далее идут имена терминалов через '|', завершённые
// нулём; логические значения по байту; при необходимости байт
// выравнивания до чётного смещения; числа по 16 или 32 бита; 16-битные
// смещения строк; таблица строк. Отрицательные числа и смещения означают
// отсутствие или отмену. Необязательная расширенная секция
// пользовательских возможностей начинается с чётного смещения. Её
// заголовок содержит пять 16-битных чисел: количества логических,
// числовых и строковых возможностей, число элементов таблицы строк и её
// размер. Затем идут логические значения, выравнивание, числа, строковые
// смещения и смещения имён возможностей в порядке логических, числовых и
// строковых. Имена находятся в таблице непосредственно после значений
// строк, смещения отсчитываются от начала имён. Завершает секцию таблица
// строк.
//
//
//
//
//
//
//
//
//
//
//
//

namespace
{

class Reader
{
    const uint8_t *data;
    size_t size;
    size_t pos {0};

public:

    bool ok {true};

    Reader(const char *aData, size_t aSize) noexcept :
        data((const uint8_t *) aData),
        size(aSize)
    {
    }

    size_t tell() const noexcept { return pos; }
    size_t remaining() const noexcept { return pos <= size ? size - pos : 0; }

    void alignEven() noexcept
    {
        if (pos % 2 != 0)
            skip(1);
    }

    void skip(size_t n) noexcept
    {
        if (remaining() < n)
            ok = false, pos = size;
        else
            pos += n;
    }

    const uint8_t *take(size_t n) noexcept
    {
        if (remaining() < n)
        {
            ok = false;
            pos = size;
            return nullptr;
        }
        const uint8_t *p = &data[pos];
        pos += n;
        return p;
    }

    int16_t i16() noexcept
    {
        if (const uint8_t *p = take(2))
            return (int16_t) (uint16_t) (p[0] | (p[1] << 8));
        return -1;
    }

    int32_t i32() noexcept
    {
        if (const uint8_t *p = take(4))
            return (int32_t) ((uint32_t) p[0] | ((uint32_t) p[1] << 8) |
                              ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24));
        return -1;
    }
};

} // пространство имён

static bool getTableString( const uint8_t *table, size_t tableSize,
                            int offset, std::string &result ) noexcept
{
    if (offset < 0 || (size_t) offset >= tableSize)
        return false;
    const uint8_t *begin = &table[offset];
    const uint8_t *end = (const uint8_t *) memchr(begin, '\0', tableSize - offset);
    if (!end)
        return false;
    result.assign((const char *) begin, end - begin);
    return true;
}

bool TermInfo::parse(const char *aData, size_t aSize, TermInfo &ti) noexcept
{
    enum { magicLegacy = 0432, magicExtNum = 01036 };

    Reader r(aData, aSize);
    int magic = r.i16();
    if (!r.ok || (magic != magicLegacy && magic != magicExtNum))
        return false;
    const bool numIs32 = (magic == magicExtNum);
    int nameSize = r.i16(),
        boolCount = r.i16(),
        numCount = r.i16(),
        strCount = r.i16(),
        strSize = r.i16();
    if ( !r.ok || nameSize < 0 || boolCount < 0 || numCount < 0
         || strCount < 0 || strSize < 0 )
        return false;

    ti.bools.clear();
    ti.nums.clear();
    ti.strs.clear();

    r.skip(nameSize);
    // Booleans.
    {
        const uint8_t *b = r.take(boolCount);
        if (!b)
            return false;
        for (int i = 0; i < boolCount; ++i)
            if (b[i] == 1 && (size_t) i < boolNameCount)
                ti.bools.emplace_back(boolNames[i], true);
    }
    r.alignEven();
    // Numbers.
    for (int i = 0; i < numCount; ++i)
    {
        int32_t n = numIs32 ? r.i32() : r.i16();
        if (n >= 0 && (size_t) i < numNameCount)
            ti.nums.emplace_back(numNames[i], (int) n);
    }
    // Strings.
    {
        std::vector<int> offsets(strCount);
        for (int &off : offsets)
            off = r.i16();
        const uint8_t *table = r.take(strSize);
        if (!r.ok)
            return false;
        for (int i = 0; i < strCount; ++i)
        {
            std::string value;
            if ( (size_t) i < strNameCount &&
                 getTableString(table, strSize, offsets[i], value) )
                ti.strs.push_back({strNames[i], std::move(value)});
        }
    }

    // Расширенная секция, необязательная.
    r.alignEven();
    if (r.remaining() >= 10)
    {
        int extBoolCount = r.i16(),
            extNumCount = r.i16(),
            extStrCount = r.i16(),
            extStrUsage = r.i16(),
            extStrSize = r.i16();
        (void) extStrUsage;
        if ( !r.ok || extBoolCount < 0 || extNumCount < 0 || extStrCount < 0
             || extStrSize < 0 )
            return true; // Игнорируем повреждённую расширенную секцию.

        std::vector<uint8_t> boolValues;
        if (const uint8_t *b = r.take(extBoolCount))
            boolValues.assign(b, b + extBoolCount);
        r.alignEven();
        std::vector<int32_t> numValues(extNumCount);
        for (auto &n : numValues)
            n = numIs32 ? r.i32() : r.i16();
        std::vector<int> strOffsets(extStrCount);
        for (int &off : strOffsets)
            off = r.i16();
        std::vector<int> nameOffsets(extBoolCount + extNumCount + extStrCount);
        for (int &off : nameOffsets)
            off = r.i16();
        const uint8_t *table = r.take(extStrSize);
        if (!r.ok)
            return true;

        // Имена начинаются непосредственно после последнего строкового значения.
        size_t namesBegin = 0;
        for (int off : strOffsets)
            if (off >= 0 && off < extStrSize)
                if (auto *end = (const uint8_t *) memchr(&table[off], '\0', extStrSize - off))
                    namesBegin = max<size_t>(namesBegin, end - table + 1);
        const uint8_t *names = table + namesBegin;
        size_t namesSize = extStrSize - namesBegin;

        size_t n = 0;
        std::string name;
        for (int i = 0; i < extBoolCount; ++i, ++n)
            if (getTableString(names, namesSize, nameOffsets[n], name) && boolValues[i] == 1)
                ti.bools.emplace_back(name, true);
        for (int i = 0; i < extNumCount; ++i, ++n)
            if (getTableString(names, namesSize, nameOffsets[n], name) && numValues[i] >= 0)
                ti.nums.emplace_back(name, (int) numValues[i]);
        for (int i = 0; i < extStrCount; ++i, ++n)
        {
            std::string value;
            if ( getTableString(names, namesSize, nameOffsets[n], name) &&
                 getTableString(table, extStrSize, strOffsets[i], value) )
                ti.strs.push_back({name, std::move(value)});
        }
    }
    return true;
}

// Поиск в базе данных.
//

static bool readFile(const std::string &path, std::string &result) noexcept
{
    // Скомпилированные записи невелики: Ncurses ограничивает их 32 КиБ либо
    // 128 КиБ в расширенном формате.
    enum { maxSize = 1 << 20 };
    bool ok = false;
    if (FILE *f = fopen(path.c_str(), "rb"))
    {
        char buf[4096];
        size_t n;
        result.clear();
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0 && result.size() < maxSize)
            result.append(buf, n);
        ok = !ferror(f) && !result.empty();
        fclose(f);
    }
    return ok;
}

static bool tryDirectory(const std::string &dir, const char *termName, TermInfo &ti) noexcept
{
    if (dir.empty())
        return false;
    char first = termName[0];
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", (unsigned char) first);
    // Есть две схемы размещения: x/xterm в большинстве систем и 78/xterm в
    // регистронезависимых файловых системах, например macOS.
    for (const char *subdir : {(const char *) nullptr, (const char *) hex})
    {
        std::string path = dir;
        path += '/';
        if (subdir)
            path += subdir;
        else
            path += first;
        path += '/';
        path += termName;
        std::string data;
        if (readFile(path, data) && TermInfo::parse(data.data(), data.size(), ti))
        {
            ti.sourcePath = path;
            return true;
        }
    }
    return false;
}

// Стандартные системные каталоги базы terminfo. Список охватывает
// распространённые дистрибутивы и менеджеры пакетов Ncurses.
static const char *const defaultDirs[] =
{
    "/etc/terminfo",
    "/lib/terminfo",
    "/usr/share/terminfo",
    "/usr/lib/terminfo",
    "/usr/share/lib/terminfo",
    "/usr/local/share/terminfo",
    "/usr/local/lib/terminfo",
    "/usr/pkg/share/terminfo",
    "/opt/local/share/terminfo",
    "/opt/homebrew/share/terminfo",
    "/opt/homebrew/opt/ncurses/share/terminfo",
    "/usr/local/opt/ncurses/share/terminfo",
    "/run/current-system/sw/share/terminfo",
    "/data/data/com.termux/files/usr/share/terminfo",
};

bool TermInfo::loadFromDatabase(const char *termName, TermInfo &ti) noexcept
{
    if (!termName || !*termName || strchr(termName, '/') || strlen(termName) > 255)
        return false;

    // Порядок поиска как в Ncurses: $TERMINFO; $HOME/.terminfo; каталоги из
    // $TERMINFO_DIRS, где пустой элемент означает системные каталоги; затем
    // системные каталоги.
    //
    //
    //
    if (const char *env = getenv("TERMINFO"))
        if (tryDirectory(env, termName, ti))
            return true;
    if (const char *home = getenv("HOME"))
        if (*home && tryDirectory(std::string(home) + "/.terminfo", termName, ti))
            return true;
    if (const char *dirs = getenv("TERMINFO_DIRS"))
    {
        const char *item = dirs;
        while (true)
        {
            const char *sep = strchr(item, ':');
            size_t len = sep ? size_t(sep - item) : strlen(item);
            if (len == 0)
            {
                for (const char *dir : defaultDirs)
                    if (tryDirectory(dir, termName, ti))
                        return true;
            }
            else if (tryDirectory(std::string(item, len), termName, ti))
                return true;
            if (!sep)
                break;
            item = sep + 1;
        }
    }
    for (const char *dir : defaultDirs)
        if (tryDirectory(dir, termName, ti))
            return true;
    if (const char *prefix = getenv("PREFIX")) // Termux и подобные среды.
        if (*prefix && tryDirectory(std::string(prefix) + "/share/terminfo", termName, ti))
            return true;
    return false;
}

// Встроенные описания используются, если тип терминала отсутствует в
// базе.

#include "terminfo_builtin.h"

void TermInfo::builtin(const char *termName, TermInfo &ti) noexcept
{
    ti.bools.clear();
    ti.nums.clear();
    ti.strs.clear();
    ti.sourcePath.clear();

    const char *name = termName ? termName : "";
    const BuiltinEntry *entry = &builtinXTerm;
    if (strncmp(name, "linux", 5) == 0)
        entry = &builtinLinux;
    for (const auto &cap : entry->strs)
        ti.strs.push_back({cap.name, cap.value});
    for (const auto &cap : entry->nums)
        ti.nums.emplace_back(cap.name, cap.value);
    // Терминал с именем вида something-256color, скорее всего, поддерживает
    // 256 цветов. Иначе предполагаем значение записи по умолчанию.
    if (entry == &builtinXTerm)
    {
        int colors = 8;
        if (strstr(name, "256color"))
            colors = 256;
        for (auto &num : ti.nums)
            if (num.first == "colors")
                num.second = colors;
            else if (num.first == "pairs")
                num.second = colors == 256 ? 65536 : 64;
    }
}

/////////////////////////////////////////////////////////////////////////
// Accessors.

const TermInfo &TermInfo::get(const char *termName) noexcept
{
    // Тип терминала обычно постоянен в течение жизни процесса, но может
    // измениться, например при изменении TERM приложением.
    static std::vector<TermInfo *> cache;
    std::string name = termName ? termName : "";
    for (TermInfo *ti : cache)
        if (ti->termName == name)
            return *ti;
    TermInfo *ti = new TermInfo;
    ti->termName = name;
    if (!loadFromDatabase(name.c_str(), *ti))
        builtin(name.c_str(), *ti);
    cache.push_back(ti);
    return *ti;
}

bool TermInfo::getBool(const char *name) const noexcept
{
    for (const auto &b : bools)
        if (b.first == name)
            return b.second;
    return false;
}

int TermInfo::getNum(const char *name) const noexcept
{
    for (const auto &n : nums)
        if (n.first == name)
            return n.second;
    return -1;
}

const char *TermInfo::getStr(const char *name) const noexcept
{
    for (const auto &s : strs)
        if (s.name == name)
            return s.value.c_str();
    return nullptr;
}

// Параметризованные строки. Реализует язык из раздела Parameterized
// Strings справки terminfo(5): '%%', '%c', '%s',
// '%[[:]flags][width[.precision]][doxX]', '%p[1-9]', '%P[a-z]',
// '%g[a-z]', '%P[A-Z]', '%g[A-Z]', '%'c'', '%{nn}', '%l', '%+', '%-',
// '%*', '%/', '%m', '%&', '%|', '%^', '%=', '%>', '%<', '%A', '%O',
// '%!', '%~', '%i' и '%? expr %t then %e else %;'.
//
//

namespace
{

class TParmStack
{
    enum { maxSize = 32 };
    int data[maxSize];
    int size {0};

public:

    void push(int v) noexcept { if (size < maxSize) data[size++] = v; }
    int pop() noexcept { return size > 0 ? data[--size] : 0; }
};

} // пространство имён

// Пропускает до соответствующего '%e' или '%;' при elseToo, иначе до
// '%;', учитывая вложенные условия. Возвращает позицию после него.
static const char *skipConditional(const char *s, bool stopAtElse) noexcept
{
    int depth = 0;
    while (*s)
    {
        if (s[0] == '%' && s[1])
        {
            char c = s[1];
            s += 2;
            if (c == '?')
                ++depth;
            else if (c == ';')
            {
                if (depth == 0)
                    return s;
                --depth;
            }
            else if (c == 'e' && depth == 0 && stopAtElse)
                return s;
        }
        else
            ++s;
    }
    return s;
}

std::string TermInfo::tparm(const char *str, const int *params, int paramCount) noexcept
{
    std::string out;
    if (!str)
        return out;
    int p[9] {};
    for (int i = 0; i < paramCount && i < 9; ++i)
        p[i] = params[i];
    static int staticVars[26];
    int dynamicVars[26] {};
    TParmStack stack;
    const char *s = str;
    while (*s)
    {
        if (*s != '%')
        {
            out += *s++;
            continue;
        }
        ++s;
        char c = *s;
        if (!c)
            break;
        ++s;
        switch (c)
        {
            case '%': out += '%'; break;
            case 'c':
            {
                // Как Ncurses, выводим нулевой символ в виде '\200'.
                char ch = (char) stack.pop();
                out += ch ? ch : '\200';
                break;
            }
            case 's':
            {
                // Строковые параметры не поддерживаются; выводим число.
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", stack.pop());
                out += buf;
                break;
            }
            case 'p':
                if ('1' <= *s && *s <= '9')
                    stack.push(p[*s++ - '1']);
                break;
            case 'P':
                if ('a' <= *s && *s <= 'z')
                    dynamicVars[*s++ - 'a'] = stack.pop();
                else if ('A' <= *s && *s <= 'Z')
                    staticVars[*s++ - 'A'] = stack.pop();
                break;
            case 'g':
                if ('a' <= *s && *s <= 'z')
                    stack.push(dynamicVars[*s++ - 'a']);
                else if ('A' <= *s && *s <= 'Z')
                    stack.push(staticVars[*s++ - 'A']);
                break;
            case '\'':
                if (*s)
                {
                    stack.push((unsigned char) *s++);
                    if (*s == '\'')
                        ++s;
                }
                break;
            case '{':
            {
                unsigned n = 0;
                while ('0' <= *s && *s <= '9')
                    n = 10*n + (unsigned) (*s++ - '0');
                if (*s == '}')
                    ++s;
                stack.push((int) n);
                break;
            }
            case 'l': stack.pop(); stack.push(0); break; // Строки не поддерживаются. При арифметическом переполнении выполняется
            // циклический переход.
            case '+': { unsigned b = stack.pop(), a = stack.pop(); stack.push((int) (a + b)); break; }
            case '-': { unsigned b = stack.pop(), a = stack.pop(); stack.push((int) (a - b)); break; }
            case '*': { unsigned b = stack.pop(), a = stack.pop(); stack.push((int) (a * b)); break; }
            case '/': { int b = stack.pop(), a = stack.pop(); stack.push(b && !(a == INT_MIN && b == -1) ? a / b : 0); break; }
            case 'm': { int b = stack.pop(), a = stack.pop(); stack.push(b && !(a == INT_MIN && b == -1) ? a % b : 0); break; }
            case '&': { int b = stack.pop(), a = stack.pop(); stack.push(a & b); break; }
            case '|': { int b = stack.pop(), a = stack.pop(); stack.push(a | b); break; }
            case '^': { int b = stack.pop(), a = stack.pop(); stack.push(a ^ b); break; }
            case '=': { int b = stack.pop(), a = stack.pop(); stack.push(a == b); break; }
            case '>': { int b = stack.pop(), a = stack.pop(); stack.push(a > b); break; }
            case '<': { int b = stack.pop(), a = stack.pop(); stack.push(a < b); break; }
            case 'A': { int b = stack.pop(), a = stack.pop(); stack.push(a && b); break; }
            case 'O': { int b = stack.pop(), a = stack.pop(); stack.push(a || b); break; }
            case '!': stack.push(!stack.pop()); break;
            case '~': stack.push(~stack.pop()); break;
            case 'i': p[0] = (int) ((unsigned) p[0] + 1); p[1] = (int) ((unsigned) p[1] + 1); break;
            case '?': break;
            case 't':
                if (!stack.pop())
                    s = skipConditional(s, true);
                break;
            case 'e':
                // Достигнут конец ветви then: пропускаем else.
                s = skipConditional(s, false);
                break;
            case ';': break;
            default:
            {
                // Формат в стиле printf: %[[:]flags][width[.precision]][doxX].
                const char *begin = s - 1;
                const char *q = begin;
                if (*q == ':')
                    ++q;
                while (*q == '-' || *q == '+' || *q == '#' || *q == ' ')
                    ++q;
                constexpr unsigned maxFormattedSize = 1 << 20;
                auto scanNumber = [&] {
                    unsigned n = 0;
                    while ('0' <= *q && *q <= '9')
                    {
                        unsigned digit = (unsigned) (*q++ - '0');
                        if (n > (maxFormattedSize - digit) / 10)
                            return false;
                        n = n * 10 + digit;
                    }
                    return true;
                };
                if (!scanNumber())
                    return {};
                if (*q == '.')
                {
                    ++q;
                    if (!scanNumber())
                        return {};
                }
                if (*q == 'd' || *q == 'o' || *q == 'x' || *q == 'X')
                {
                    char fmt[32] = "%";
                    const char *f = begin;
                    if (*f == ':')
                        ++f;
                    size_t len = (size_t) (q - f);
                    if (len < sizeof(fmt) - 3)
                    {
                        memcpy(fmt + 1, f, len);
                        fmt[len + 1] = *q;
                        fmt[len + 2] = '\0';
                        char buf[64];
                        int value = stack.pop();
                        int needed = snprintf(buf, sizeof(buf), fmt, value);
                        // Ограничиваем объём работы с повреждёнными или недоверенными описаниями терминала.
                        if (needed < 0 || (unsigned) needed > maxFormattedSize)
                            return {};
                        if ((size_t) needed < sizeof(buf))
                            out.append(buf, (size_t) needed);
                        else
                        {
                            std::vector<char> expanded((size_t) needed + 1);
                            snprintf(expanded.data(), expanded.size(), fmt, value);
                            out.append(expanded.data(), (size_t) needed);
                        }
                    }
                    s = q + 1;
                }
                break;
            }
        }
    }
    return out;
}

bool TermInfo::isStandardStrCap(const char *name) noexcept
{
    for (const char *strName : strNames)
        if (strcmp(strName, name) == 0)
            return true;
    return false;
}

int TermInfo::getColorCount() const noexcept
{
    // Аналог has_colors() с последующим start_color() в Ncurses.
    int colors = getNum("colors");
    int pairs = getNum("pairs");
    bool canSetColors =
        (getStr("setf") && getStr("setb")) ||
        (getStr("setaf") && getStr("setab")) ||
        getStr("scp");
    if (colors >= 0 && pairs >= 0 && canSetColors)
        return colors;
    return 0;
}

} // пространство имён tvision

#endif // _TV_UNIX
