#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <tvision/internal/filesave.h>
#include <tvstudio/model.h>

namespace tvstudio
{
std::string utf8(const fs::path &p)
{
    auto s = p.u8string();
    return {reinterpret_cast<const char *>(s.data()), s.size()};
}
fs::path pathFromUtf8(const std::string &s)
{
    return fs::u8path(s);
}
std::string trim(std::string s)
{
    auto white = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    auto first = std::find_if_not(s.begin(), s.end(), white),
         last = std::find_if_not(s.rbegin(), s.rend(), white).base();
    return first < last ? std::string(first, last) : std::string();
}
std::string readFile(const fs::path &path, size_t limit)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Не удалось открыть " + utf8(path));
    std::string result;
    char block[65536];
    while (in)
    {
        in.read(block, sizeof block);
        auto n = static_cast<size_t>(in.gcount());
        if (n > limit - result.size())
            throw std::runtime_error("Слишком большой файл: " + utf8(path));
        result.append(block, n);
    }
    if (in.bad())
        throw std::runtime_error("Ошибка чтения " + utf8(path));
    return result;
}
void writeFile(const fs::path &path, const std::string &text, bool backup)
{
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path());
    auto name = utf8(path), bak = name + ".bak";
    if (!tvision::saveFileAtomic(name.c_str(), backup ? bak.c_str() : nullptr, text.data(),
                                 text.size(), nullptr, 0))
        throw std::runtime_error("Не удалось сохранить " + name);
}
std::vector<std::string> splitLines(const std::string &text)
{
    std::vector<std::string> out;
    size_t begin = 0;
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\n')
        {
            out.push_back(text.substr(begin, i - begin));
            if (!out.back().empty() && out.back().back() == '\r')
                out.back().pop_back();
            begin = i + 1;
        }
    if (begin < text.size())
        out.push_back(text.substr(begin));
    return out;
}
std::string joinLines(const std::vector<std::string> &lines)
{
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i)
            out += '\n';
        out += lines[i];
    }
    return out;
}
bool within(const fs::path &path, const fs::path &directory)
{
    std::error_code ec;
    auto a = fs::weakly_canonical(path, ec);
    if (ec)
        return false;
    auto b = fs::weakly_canonical(directory, ec);
    if (ec)
        return false;
    auto i = a.begin();
    for (auto j = b.begin(); j != b.end(); ++j, ++i)
    {
        if (i == a.end())
            return false;
#ifdef _WIN32
        auto x = i->wstring(), y = j->wstring();
        std::transform(x.begin(), x.end(), x.begin(), ::towlower);
        std::transform(y.begin(), y.end(), y.begin(), ::towlower);
        if (x != y)
            return false;
#else
        if (*i != *j)
            return false;
#endif
    }
    return true;
}
bool identifier(const std::string &s)
{
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_'))
        return false;
    for (unsigned char c : s)
        if (c >= 128 || !(std::isalnum(c) || c == '_'))
            return false;
    static const std::set<std::string> reserved = {"alignas",      "alignof",
                                                   "and",          "and_eq",
                                                   "asm",          "auto",
                                                   "bitand",       "bitor",
                                                   "bool",         "break",
                                                   "case",         "catch",
                                                   "char",         "char16_t",
                                                   "char32_t",     "class",
                                                   "compl",        "const",
                                                   "constexpr",    "const_cast",
                                                   "continue",     "decltype",
                                                   "default",      "delete",
                                                   "do",           "double",
                                                   "dynamic_cast", "else",
                                                   "enum",         "explicit",
                                                   "export",       "extern",
                                                   "false",        "float",
                                                   "for",          "friend",
                                                   "goto",         "if",
                                                   "inline",       "int",
                                                   "long",         "mutable",
                                                   "namespace",    "new",
                                                   "noexcept",     "not",
                                                   "not_eq",       "nullptr",
                                                   "operator",     "or",
                                                   "or_eq",        "private",
                                                   "protected",    "public",
                                                   "register",     "reinterpret_cast",
                                                   "return",       "short",
                                                   "signed",       "sizeof",
                                                   "static",       "static_assert",
                                                   "static_cast",  "struct",
                                                   "switch",       "template",
                                                   "this",         "thread_local",
                                                   "throw",        "true",
                                                   "try",          "typedef",
                                                   "typeid",       "typename",
                                                   "union",        "unsigned",
                                                   "using",        "virtual",
                                                   "void",         "volatile",
                                                   "wchar_t",      "while",
                                                   "xor",          "xor_eq",
                                                   "concept",      "requires",
                                                   "co_await",     "co_return",
                                                   "co_yield"};
    return !reserved.count(s) && s.find("__") == std::string::npos &&
           !(s.size() > 1 && s[0] == '_' && std::isupper(static_cast<unsigned char>(s[1])));
}
std::string cppName(const std::string &s)
{
    std::string out;
    for (unsigned char c : s)
        out += (c < 128 && (std::isalnum(c) || c == '_')) ? char(c) : '_';
    if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0])))
        out = "app_" + out;
    if (!identifier(out))
        out = "app_" + out;
    return out;
}
std::string quotedCpp(const std::string &s)
{
    std::string out = "\"";
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 32 || c == 127)
            {
                out += '\\';
                out += char('0' + ((c >> 6) & 7));
                out += char('0' + ((c >> 3) & 7));
                out += char('0' + (c & 7));
            }
            else
                out += char(c);
        }
    }
    return out + '"';
}
const Json &get(const Json &o, const std::string &key)
{
    static const Json empty;
    auto *p = o.find(key);
    return p ? *p : empty;
}
std::string str(const Json &o, const std::string &key, std::string fallback)
{
    auto *p = o.find(key);
    if (p && p->kind != Json::Kind::Null && p->kind != Json::Kind::String)
        throw std::runtime_error("Ожидается строковое поле: " + key);
    return p && p->kind == Json::Kind::String ? p->text : std::move(fallback);
}
int64_t integer(const Json &o, const std::string &key, int64_t fallback)
{
    auto *p = o.find(key);
    if (!p)
        return fallback;
    if (p->kind != Json::Kind::Number)
        throw std::runtime_error("Ожидается целое поле: " + key);
    size_t n = 0;
    try
    {
        auto v = std::stoll(p->text, &n);
        if (n == p->text.size())
            return v;
    }
    catch (const std::exception &)
    {
    }
    throw std::runtime_error("Недопустимое целое поле: " + key);
}
bool boolean(const Json &o, const std::string &key, bool fallback)
{
    auto *p = o.find(key);
    if (!p)
        return fallback;
    if (p->kind != Json::Kind::Boolean)
        throw std::runtime_error("Ожидается логическое поле: " + key);
    return p->boolean;
}
void put(Json &o, std::string key, Json value)
{
    if (o.kind != Json::Kind::Object)
        o = Json::record();
    if (auto *p = o.find(key))
        *p = std::move(value);
    else
        o.object.emplace_back(std::move(key), std::move(value));
}

namespace
{
size_t separator(const std::string &s, char wanted, bool block = false)
{
    char quote = 0;
    int depth = 0;
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (quote)
        {
            if (c == '\\' && quote == '"')
                ++i;
            else if (c == quote)
            {
                if (quote == '\'' && i + 1 < s.size() && s[i + 1] == '\'')
                    ++i;
                else
                    quote = 0;
            }
            continue;
        }
        if (c == '\'' || c == '"')
        {
            quote = c;
            continue;
        }
        if (c == '[' || c == '{')
            ++depth;
        else if (c == ']' || c == '}')
            --depth;
        else if (c == wanted && depth == 0 &&
                 (!block || i + 1 == s.size() ||
                  std::isspace(static_cast<unsigned char>(s[i + 1]))))
            return i;
    }
    return std::string::npos;
}
std::string withoutComment(std::string s)
{
    char q = 0;
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (q)
        {
            if (c == '\\' && q == '"')
                ++i;
            else if (c == q)
            {
                if (q == '\'' && i + 1 < s.size() && s[i + 1] == '\'')
                    ++i;
                else
                    q = 0;
            }
        }
        else if (c == '\'' || c == '"')
            q = c;
        else if (c == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(s[i - 1]))))
        {
            s.resize(i);
            break;
        }
    }
    return trim(s);
}
Json scalar(std::string s, unsigned depth = 0)
{
    if (depth > 100)
        throw std::runtime_error("YAML nesting limit");
    s = trim(s);
    if (s.empty() || s == "null" || s == "~")
        return {};
    if (s[0] == '"')
        return tvision::parseJson(s);
    if (s[0] == '\'')
    {
        if (s.size() < 2 || s.back() != '\'')
            throw std::runtime_error("Незакрытая строка YAML");
        std::string out;
        for (size_t i = 1; i + 1 < s.size(); ++i)
        {
            out += s[i];
            if (s[i] == '\'' && i + 2 < s.size() && s[i + 1] == '\'')
                ++i;
        }
        return Json::string(out);
    }
    if (s == "true" || s == "True" || s == "TRUE")
        return Json::flag(true);
    if (s == "false" || s == "False" || s == "FALSE")
        return Json::flag(false);
    if (s[0] == '[' || s[0] == '{')
    {
        bool object = s[0] == '{';
        if (s.back() != (object ? '}' : ']'))
            throw std::runtime_error("Незакрытая коллекция YAML");
        Json out = object ? Json::record() : Json::list();
        std::string body = s.substr(1, s.size() - 2);
        while (!trim(body).empty())
        {
            auto p = separator(body, ',');
            std::string item = p == std::string::npos ? body : body.substr(0, p);
            if (object)
            {
                auto c = separator(item, ':');
                if (c == std::string::npos)
                    throw std::runtime_error("Ожидается ключ YAML");
                std::string key = trim(item.substr(0, c));
                if (!key.empty() && (key[0] == '"' || key[0] == '\''))
                    key = scalar(key, depth + 1).text;
                if (out.find(key))
                    throw std::runtime_error("Повторный ключ YAML: " + key);
                put(out, key, scalar(item.substr(c + 1), depth + 1));
            }
            else
                out.array.push_back(scalar(item, depth + 1));
            if (p == std::string::npos)
                break;
            body = body.substr(p + 1);
        }
        return out;
    }
    if (s[0] == '&' || s[0] == '*' || s[0] == '!')
        throw std::runtime_error("Якоря, ссылки и теги YAML не поддерживаются");
    if (s[0] == '-' || std::isdigit(static_cast<unsigned char>(s[0])))
    {
        try
        {
            auto n = tvision::parseJson(s);
            if (n.kind == Json::Kind::Number)
                return n;
        }
        catch (const tvision::JsonError &)
        {
        }
    }
    return Json::string(s);
}
class YamlReader
{
    struct Line
    {
        int indent;
        std::string text, raw;
        size_t number;
    };
    std::vector<Line> lines_;
    size_t at_ = 0;
    unsigned nesting_ = 0;
    [[noreturn]] void error(const std::string &text, size_t line) const
    {
        throw std::runtime_error("YAML, строка " + std::to_string(line) + ": " + text);
    }
    bool sequence(const std::string &s) const
    {
        return s == "-" || (s.size() > 1 && s[0] == '-' && s[1] == ' ');
    }
    Json value(std::string text, int indent, size_t line)
    {
        if (text == "|" || text == "|-" || text == "|+" || text == ">" || text == ">-" ||
            text == ">+")
        {
            bool fold = text[0] == '>';
            std::string out;
            int contentIndent = -1;
            while (at_ < lines_.size() && (lines_[at_].indent > indent || lines_[at_].text.empty()))
            {
                auto &part = lines_[at_++];
                if (contentIndent < 0 && !part.text.empty())
                    contentIndent = part.indent;
                if (!out.empty() && fold && !part.text.empty())
                    out += ' ';
                out += part.raw.substr(
                    std::min<size_t>(part.raw.size(), size_t(std::max(0, contentIndent))));
                if (!fold || part.text.empty())
                    out += '\n';
            }
            if (text.back() == '-')
            {
                while (!out.empty() && out.back() == '\n')
                    out.pop_back();
            }
            else if (text.back() != '+')
            {
                while (!out.empty() && out.back() == '\n')
                    out.pop_back();
                out += '\n';
            }
            return Json::string(out);
        }
        if (text.empty())
        {
            skip();
            if (at_ < lines_.size() && lines_[at_].indent > indent)
                return block(lines_[at_].indent, 1);
            return {};
        }
        try
        {
            return scalar(text);
        }
        catch (const std::exception &e)
        {
            error(e.what(), line);
        }
    }
    void skip()
    {
        while (at_ < lines_.size() && lines_[at_].text.empty())
            ++at_;
    }
    void entry(Json &object, const std::string &text, int indent, size_t line)
    {
        auto c = separator(text, ':', true);
        if (c == std::string::npos)
            error("Ожидается 'ключ: значение'", line);
        std::string key = trim(text.substr(0, c));
        if (!key.empty() && (key[0] == '\'' || key[0] == '"'))
            key = scalar(key).text;
        if (key.empty() || object.find(key))
            error("Пустой или повторный ключ: " + key, line);
        put(object, key, value(trim(text.substr(c + 1)), indent, line));
    }
    Json block(int indent, unsigned depth)
    {
        struct Level
        {
            unsigned &n;
            Level(unsigned &v) : n(v)
            {
                ++n;
            }
            ~Level()
            {
                --n;
            }
        } level(nesting_);
        if (nesting_ > 100)
            throw std::runtime_error("Слишком глубокая структура YAML");
        skip();
        if (at_ == lines_.size())
            return {};
        bool seq = sequence(lines_[at_].text);
        Json out = seq ? Json::list() : Json::record();
        while (true)
        {
            skip();
            if (at_ == lines_.size() || lines_[at_].indent < indent)
                break;
            auto line = lines_[at_];
            if (line.indent != indent)
                error("Неожиданный отступ", line.number);
            if (sequence(line.text) != seq)
                error("Список и словарь на одном уровне", line.number);
            ++at_;
            if (!seq)
                entry(out, line.text, indent, line.number);
            else
            {
                auto rest = trim(line.text.substr(1));
                Json item;
                if (rest.empty())
                {
                    skip();
                    item = at_ < lines_.size() && lines_[at_].indent > indent
                               ? block(lines_[at_].indent, depth + 1)
                               : Json();
                }
                else if (separator(rest, ':', true) != std::string::npos)
                {
                    item = Json::record();
                    entry(item, rest, indent + 2, line.number);
                    while (true)
                    {
                        skip();
                        if (at_ == lines_.size() || lines_[at_].indent <= indent)
                            break;
                        auto next = lines_[at_];
                        if (next.indent != indent + 2)
                            error("Неверный отступ элемента списка", next.number);
                        ++at_;
                        entry(item, next.text, next.indent, next.number);
                    }
                }
                else
                    item = value(rest, indent, line.number);
                out.array.push_back(std::move(item));
            }
        }
        return out;
    }

  public:
    Json parse(const std::string &text)
    {
        size_t number = 0;
        for (auto raw : splitLines(text))
        {
            ++number;
            if (number == 1 && raw.compare(0, 3, "\xEF\xBB\xBF") == 0)
                raw.erase(0, 3);
            int indent = 0;
            while (indent < (int)raw.size() && raw[indent] == ' ')
                ++indent;
            if (indent < (int)raw.size() && raw[indent] == '\t')
                error("Табуляция в отступе", number);
            auto body = withoutComment(raw.substr(indent));
            if (body == "---" || body == "...")
                continue;
            lines_.push_back({indent, body, raw, number});
        }
        skip();
        if (at_ == lines_.size())
            return Json::record();
        auto out = block(lines_[at_].indent, 0);
        skip();
        if (at_ != lines_.size())
            error("Лишние данные", lines_[at_].number);
        return out;
    }
};
bool compound(const Json &j)
{
    return (j.kind == Json::Kind::Object && !j.object.empty()) ||
           (j.kind == Json::Kind::Array && !j.array.empty());
}
void emit(const Json &j, int indent, std::vector<std::string> &out)
{
    std::string pad((size_t)indent, ' ');
    if (j.kind == Json::Kind::Object)
    {
        for (const auto &p : j.object)
        {
            std::string key = p.first;
            if (key.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
                std::string::npos)
                key = tvision::writeJson(Json::string(key));
            if (compound(p.second))
            {
                out.push_back(pad + key + ":");
                emit(p.second, indent + 2, out);
            }
            else
                out.push_back(pad + key + ": " + tvision::writeJson(p.second));
        }
    }
    else if (j.kind == Json::Kind::Array)
    {
        for (const auto &v : j.array)
        {
            if (compound(v))
            {
                std::vector<std::string> sub;
                emit(v, indent + 2, sub);
                out.push_back(pad + "- " + sub.front().substr(size_t(indent + 2)));
                out.insert(out.end(), sub.begin() + 1, sub.end());
            }
            else
                out.push_back(pad + "- " + tvision::writeJson(v));
        }
    }
    else
        out.push_back(pad + tvision::writeJson(j));
}
} // namespace
Json parseDocument(const std::string &text)
{
    auto s = trim(text);
    if (s.compare(0, 3, "\xEF\xBB\xBF") == 0)
        s = trim(s.substr(3));
    if (s.empty())
        return Json::record();
    if (s[0] == '{' || s[0] == '[')
        return tvision::parseJson(s);
    return YamlReader().parse(text);
}
std::string writeYaml(const Json &value)
{
    std::vector<std::string> lines;
    emit(value, 0, lines);
    return joinLines(lines) + '\n';
}
} // namespace tvstudio
