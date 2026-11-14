#include <algorithm>
#include <cstdint>
#include <set>
#include <tvision/json.h>
namespace tvision
{
Json Json::string(std::string v)
{
    Json j;
    j.kind = Kind::String;
    j.text = std::move(v);
    return j;
}
Json Json::flag(bool v)
{
    Json j;
    j.kind = Kind::Boolean;
    j.boolean = v;
    return j;
}
Json Json::list(std::vector<Json> v)
{
    Json j;
    j.kind = Kind::Array;
    j.array = std::move(v);
    return j;
}
Json Json::record(std::vector<std::pair<std::string, Json>> v)
{
    Json j;
    j.kind = Kind::Object;
    j.object = std::move(v);
    return j;
}
const Json *Json::find(const std::string &key) const
{
    for (const auto &p : object)
        if (p.first == key)
            return &p.second;
    return nullptr;
}
Json *Json::find(const std::string &key)
{
    for (auto &p : object)
        if (p.first == key)
            return &p.second;
    return nullptr;
}
JsonError::JsonError(const std::string &m, size_t o, size_t l, size_t c, bool cancel)
    : std::runtime_error(m + " at " + std::to_string(l) + ":" + std::to_string(c)), offset(o),
      line(l), column(c), cancelled(cancel)
{
}
namespace
{
void utf8(std::string &s, uint32_t c)
{
    if (c < 128)
        s += char(c);
    else if (c < 2048)
    {
        s += char(0xC0 | (c >> 6));
        s += char(0x80 | (c & 63));
    }
    else if (c < 65536)
    {
        s += char(0xE0 | (c >> 12));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    }
    else
    {
        s += char(0xF0 | (c >> 18));
        s += char(0x80 | ((c >> 12) & 63));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    }
}
class Parser
{
    const std::string &s;
    size_t i = 0, nextReport = 0;
    JsonProgress hook;
    [[noreturn]] void error(const std::string &m, bool cancel = false) const
    {
        size_t line = 1, col = 1;
        for (size_t k = 0; k < i && k < s.size(); ++k)
        {
            if (s[k] == '\n')
            {
                ++line;
                col = 1;
            }
            else
                ++col;
        }
        throw JsonError(m, i, line, col, cancel);
    }
    void report()
    {
        if (hook && i >= nextReport)
        {
            nextReport = i + 65536;
            if (!hook(i, s.size()))
                error("JSON parsing cancelled", true);
        }
    }
    char peek() const
    {
        return i < s.size() ? s[i] : '\0';
    }
    void space()
    {
        while (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')
        {
            ++i;
            report();
        }
    }
    uint32_t hex()
    {
        uint32_t n = 0;
        for (int k = 0; k < 4; ++k)
        {
            char c = peek();
            if (!c)
                error("Incomplete Unicode escape");
            ++i;
            if (c >= '0' && c <= '9')
                n = n * 16 + c - '0';
            else if (c >= 'a' && c <= 'f')
                n = n * 16 + c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                n = n * 16 + c - 'A' + 10;
            else
                error("Invalid Unicode escape");
        }
        return n;
    }
    std::string string()
    {
        if (peek() != '"')
            error("Expected string");
        ++i;
        std::string out;
        while (i < s.size())
        {
            report();
            unsigned char c = (unsigned char)s[i++];
            if (c == '"')
                return out;
            if (c < 32)
                error("Control character in string");
            if (c == '\\')
            {
                char e = peek();
                if (!e)
                    error("Incomplete escape");
                ++i;
                switch (e)
                {
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                case '/':
                    out += '/';
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u':
                {
                    uint32_t cp = hex();
                    if (cp >= 0xD800 && cp <= 0xDBFF)
                    {
                        if (i + 2 > s.size() || s[i] != '\\' || s[i + 1] != 'u')
                            error("Missing low surrogate");
                        i += 2;
                        uint32_t low = hex();
                        if (low < 0xDC00 || low > 0xDFFF)
                            error("Invalid low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    }
                    else if (cp >= 0xDC00 && cp <= 0xDFFF)
                        error("Unexpected low surrogate");
                    utf8(out, cp);
                    break;
                }
                default:
                    error("Invalid escape");
                }
            }
            else if (c < 128)
                out += char(c);
            else
            {
                size_t begin = i - 1;
                unsigned count = c >= 0xC2 && c <= 0xDF   ? 1
                                 : c >= 0xE0 && c <= 0xEF ? 2
                                 : c >= 0xF0 && c <= 0xF4 ? 3
                                                          : 0;
                if (!count)
                    error("Invalid UTF-8");
                uint32_t cp = c & ((1u << (6 - count)) - 1);
                for (unsigned k = 0; k < count; ++k)
                {
                    if (i >= s.size() || ((unsigned char)s[i] & 0xC0) != 0x80)
                        error("Invalid UTF-8");
                    cp = (cp << 6) | ((unsigned char)s[i++] & 63);
                }
                if ((count == 1 && cp < 128) || (count == 2 && cp < 2048) ||
                    (count == 3 && cp < 65536) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                    error("Invalid UTF-8 scalar");
                out.append(s, begin, i - begin);
            }
        }
        error("Unterminated string");
    }
    Json number()
    {
        size_t begin = i;
        if (peek() == '-')
            ++i;
        if (peek() == '0')
            ++i;
        else
        {
            if (peek() < '1' || peek() > '9')
                error("Expected number");
            while (peek() >= '0' && peek() <= '9')
            {
                ++i;
                report();
            }
        }
        if (peek() == '.')
        {
            ++i;
            size_t digits = i;
            while (peek() >= '0' && peek() <= '9')
            {
                ++i;
                report();
            }
            if (i == digits)
                error("Missing fraction");
        }
        if (peek() == 'e' || peek() == 'E')
        {
            ++i;
            if (peek() == '+' || peek() == '-')
                ++i;
            size_t digits = i;
            while (peek() >= '0' && peek() <= '9')
            {
                ++i;
                report();
            }
            if (i == digits)
                error("Missing exponent");
        }
        Json j;
        j.kind = Json::Kind::Number;
        j.text = s.substr(begin, i - begin);
        return j;
    }
    Json value(unsigned depth)
    {
        report();
        space();
        if (depth > 200)
            error("JSON nesting limit exceeded");
        char c = peek();
        if (c == '"')
            return Json::string(string());
        if (c == '[')
        {
            ++i;
            space();
            Json out = Json::list();
            if (peek() == ']')
            {
                ++i;
                return out;
            }
            for (;;)
            {
                out.array.push_back(value(depth + 1));
                space();
                if (peek() == ']')
                {
                    ++i;
                    return out;
                }
                if (peek() != ',')
                    error("Expected comma or ]");
                ++i;
            }
        }
        if (c == '{')
        {
            ++i;
            space();
            Json out = Json::record();
            std::set<std::string> keys;
            if (peek() == '}')
            {
                ++i;
                return out;
            }
            for (;;)
            {
                space();
                std::string key = string();
                if (!keys.insert(key).second)
                    error("Duplicate object key");
                space();
                if (peek() != ':')
                    error("Expected colon");
                ++i;
                out.object.emplace_back(std::move(key), value(depth + 1));
                space();
                if (peek() == '}')
                {
                    ++i;
                    return out;
                }
                if (peek() != ',')
                    error("Expected comma or }");
                ++i;
            }
        }
        if (s.compare(i, 4, "null") == 0)
        {
            i += 4;
            return {};
        }
        if (s.compare(i, 4, "true") == 0)
        {
            i += 4;
            return Json::flag(true);
        }
        if (s.compare(i, 5, "false") == 0)
        {
            i += 5;
            return Json::flag(false);
        }
        if (c == '-' || (c >= '0' && c <= '9'))
            return number();
        error("Expected JSON value");
    }

  public:
    Parser(const std::string &text, JsonProgress h) : s(text), hook(std::move(h)) {}
    Json parse()
    {
        if (s.compare(0, 3, "\xEF\xBB\xBF") == 0)
            i = 3;
        Json out = value(0);
        space();
        if (i != s.size())
            error("Trailing JSON data");
        if (hook && !hook(s.size(), s.size()))
            error("JSON parsing cancelled", true);
        return out;
    }
};
std::string quoted(const std::string &s)
{
    std::string out = "\"";
    const char *hex = "0123456789abcdef";
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
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
            if (c < 32)
            {
                out += "\\u00";
                out += hex[c >> 4];
                out += hex[c & 15];
            }
            else
                out += char(c);
        }
    }
    return out + '"';
}
std::string dump(const Json &j, bool pretty, unsigned depth)
{
    if (depth > 200)
        throw std::invalid_argument("JSON nesting limit exceeded");
    switch (j.kind)
    {
    case Json::Kind::Null:
        return "null";
    case Json::Kind::Boolean:
        return j.boolean ? "true" : "false";
    case Json::Kind::Number:
        return j.text;
    case Json::Kind::String:
        return quoted(j.text);
    default:
        break;
    }
    bool obj = j.kind == Json::Kind::Object;
    size_t n = obj ? j.object.size() : j.array.size();
    std::string out = obj ? "{" : "[";
    for (size_t i = 0; i < n; ++i)
    {
        if (i)
            out += ',';
        if (pretty)
        {
            out += '\n';
            out.append((depth + 1) * 2, ' ');
        }
        if (obj)
            out += quoted(j.object[i].first) + (pretty ? ": " : ":");
        out += dump(obj ? j.object[i].second : j.array[i], pretty, depth + 1);
    }
    if (pretty && n)
    {
        out += '\n';
        out.append(depth * 2, ' ');
    }
    return out + (obj ? "}" : "]");
}
} // namespace
Json parseJson(const std::string &text, JsonProgress progress)
{
    return Parser(text, std::move(progress)).parse();
}
Json Json::number(std::string value)
{
    Json j = parseJson(value);
    if (j.kind != Kind::Number)
        throw std::invalid_argument("Expected JSON number");
    return j;
}
std::string writeJson(const Json &value, bool pretty)
{
    return dump(value, pretty, 0);
}
std::string Json::display() const
{
    return kind == Kind::String ? text : writeJson(*this);
}
JsonTableModel::JsonTableModel(Json value) : data_(std::move(value))
{
    if (data_.kind != Json::Kind::Array)
        throw std::invalid_argument("JSON table expects an array of objects");
    std::set<std::string> seen;
    for (const auto &row : data_.array)
    {
        if (row.kind != Json::Kind::Object)
            throw std::invalid_argument("JSON table expects objects");
        for (const auto &p : row.object)
            if (seen.insert(p.first).second)
                columns_.push_back(p.first);
    }
}
std::string JsonTableModel::header(size_t c) const
{
    return c < columns_.size() ? columns_[c] : columnLetters(c);
}
std::string JsonTableModel::cell(size_t r, size_t c) const
{
    if (r >= rowCount() || c >= columnCount())
        return "";
    auto *v = data_.array[r].find(columns_[c]);
    return v ? v->display() : "";
}
bool JsonTableModel::setCell(size_t r, size_t c, const std::string &text)
{
    if (!editable(r, c))
        return false;
    auto *old = data_.array[r].find(columns_[c]);
    Json v;
    try
    {
        if (!old || old->kind == Json::Kind::String)
            v = Json::string(text);
        else
        {
            v = parseJson(text);
            if (v.kind != old->kind)
                return false;
        }
    }
    catch (const JsonError &)
    {
        return false;
    }
    if (old)
        *old = std::move(v);
    else
        data_.array[r].object.emplace_back(columns_[c], std::move(v));
    return true;
}
bool JsonTableModel::sort(size_t c, bool asc)
{
    if (c >= columnCount())
        return false;
    const auto &key = columns_[c];
    std::stable_sort(data_.array.begin(), data_.array.end(),
                     [&](const Json &a, const Json &b)
                     {
                         auto *x = a.find(key), *y = b.find(key);
                         int n = naturalCompare(x ? x->display() : "", y ? y->display() : "");
                         return asc ? n < 0 : n > 0;
                     });
    return true;
}
} // namespace tvision
