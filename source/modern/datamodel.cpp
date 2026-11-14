#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <tvision/datamodel.h>
namespace tvision
{
std::string columnLetters(size_t c)
{
    std::string s;
    for (;;)
    {
        s.push_back(char('A' + c % 26));
        if (c < 26)
            break;
        c = c / 26 - 1;
    }
    std::reverse(s.begin(), s.end());
    return s;
}
namespace
{
struct Decimal
{
    bool valid = false, negative = false;
    std::string digits;
    int64_t exponent = 0;
};
Decimal decimal(const std::string &input)
{
    Decimal n;
    size_t i = 0, end = input.size();
    while (i < end && std::isspace((unsigned char)input[i]))
        ++i;
    while (end > i && std::isspace((unsigned char)input[end - 1]))
        --end;
    if (i < end && (input[i] == '-' || input[i] == '+'))
        n.negative = input[i++] == '-';
    bool point = false, any = false;
    int64_t before = 0;
    for (; i < end; ++i)
    {
        char c = input[i];
        if (c == '.' || c == ',')
        {
            if (point)
                return n;
            point = true;
        }
        else if (c >= '0' && c <= '9')
        {
            n.digits += c;
            any = true;
            if (!point)
                ++before;
        }
        else
            break;
    }
    if (!any)
        return n;
    int64_t exp = 0;
    bool minus = false;
    if (i < end && (input[i] == 'e' || input[i] == 'E'))
    {
        ++i;
        if (i < end && (input[i] == '+' || input[i] == '-'))
            minus = input[i++] == '-';
        size_t begin = i;
        for (; i < end && input[i] >= '0' && input[i] <= '9'; ++i)
        {
            if (exp > 1000000)
                return n;
            exp = exp * 10 + input[i] - '0';
        }
        if (begin == i)
            return n;
    }
    if (i != end)
        return n;
    size_t first = n.digits.find_first_not_of('0');
    if (first == std::string::npos)
    {
        n.digits = "0";
        n.negative = false;
        n.exponent = 0;
    }
    else
    {
        n.exponent = before - (int64_t)first + (minus ? -exp : exp);
        n.digits.erase(0, first);
        while (n.digits.size() > 1 && n.digits.back() == '0')
            n.digits.pop_back();
    }
    n.valid = true;
    return n;
}
} // пространство имён
int naturalCompare(const std::string &a, const std::string &b)
{
    auto x = decimal(a), y = decimal(b);
    if (x.valid != y.valid)
        return x.valid ? -1 : 1;
    if (x.valid && y.valid)
    {
        if (x.negative != y.negative)
            return x.negative ? -1 : 1;
        int result = 0;
        if (x.digits == "0" || y.digits == "0")
            result = x.digits == y.digits ? 0 : x.digits == "0" ? -1 : 1;
        else if (x.exponent != y.exponent)
            result = x.exponent < y.exponent ? -1 : 1;
        else
            for (size_t i = 0; i < std::max(x.digits.size(), y.digits.size()); ++i)
            {
                char xc = i < x.digits.size() ? x.digits[i] : '0',
                     yc = i < y.digits.size() ? y.digits[i] : '0';
                if (xc != yc)
                {
                    result = xc < yc ? -1 : 1;
                    break;
                }
            }
        return x.negative ? -result : result;
    }
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size())
    {
        if (std::isdigit((unsigned char)a[i]) && std::isdigit((unsigned char)b[j]))
        {
            size_t ae = i, be = j;
            while (ae < a.size() && std::isdigit((unsigned char)a[ae]))
                ++ae;
            while (be < b.size() && std::isdigit((unsigned char)b[be]))
                ++be;
            size_t ai = i, bi = j;
            while (ai < ae && a[ai] == '0')
                ++ai;
            while (bi < be && b[bi] == '0')
                ++bi;
            if (ae - ai != be - bi)
                return ae - ai < be - bi ? -1 : 1;
            int c = a.compare(ai, ae - ai, b, bi, be - bi);
            if (c)
                return c < 0 ? -1 : 1;
            i = ae;
            j = be;
        }
        else
        {
            int ac = std::tolower((unsigned char)a[i++]), bc = std::tolower((unsigned char)b[j++]);
            if (ac != bc)
                return ac < bc ? -1 : 1;
        }
    }
    return i == a.size() ? (j == b.size() ? 0 : -1) : 1;
}
VecTableModel::VecTableModel(std::vector<std::string> h, std::vector<std::vector<std::string>> r)
    : headers_(std::move(h)), rows_(std::move(r)), columns_(headers_.size())
{
    for (const auto &row : rows_)
        columns_ = std::max(columns_, row.size());
}
std::string VecTableModel::cell(size_t r, size_t c) const
{
    return r < rows_.size() && c < rows_[r].size() ? rows_[r][c] : "";
}
std::string VecTableModel::header(size_t c) const
{
    return c < headers_.size() ? headers_[c] : columnLetters(c);
}
bool VecTableModel::setCell(size_t r, size_t c, const std::string &value)
{
    if (!editable(r, c))
        return false;
    if (rows_[r].size() <= c)
        rows_[r].resize(c + 1);
    rows_[r][c] = value;
    return true;
}
bool VecTableModel::sort(size_t c, bool ascending)
{
    if (c >= columns_)
        return false;
    std::stable_sort(rows_.begin(), rows_.end(),
                     [&](const std::vector<std::string> &a, const std::vector<std::string> &b)
                     {
                         int cmp =
                             naturalCompare(c < a.size() ? a[c] : "", c < b.size() ? b[c] : "");
                         return ascending ? cmp < 0 : cmp > 0;
                     });
    return true;
}
FnTableModel::FnTableModel(size_t r, size_t c, std::function<std::string(size_t, size_t)> get,
                           std::function<std::string(size_t)> h)
    : rows_(r), columns_(c), getter_(std::move(get)), header_(std::move(h))
{
    if (!getter_)
        throw std::invalid_argument("FnTableModel requires a getter");
}
std::string FnTableModel::cell(size_t r, size_t c) const
{
    return r < rows_ && c < columns_ ? getter_(r, c) : "";
}
std::string FnTableModel::header(size_t c) const
{
    return header_ ? header_(c) : columnLetters(c);
}
std::vector<std::vector<std::string>> parseCsv(const std::string &text, char delimiter)
{
    if (delimiter == '\r' || delimiter == '\n' || delimiter == '"' || delimiter == '\0')
        throw std::invalid_argument("Invalid CSV delimiter");
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool quoted = false, closed = false, pending = false;
    for (size_t i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        pending = true;
        if (quoted)
        {
            if (c == '"')
            {
                if (i + 1 < text.size() && text[i + 1] == '"')
                {
                    field += '"';
                    ++i;
                }
                else
                {
                    quoted = false;
                    closed = true;
                }
            }
            else
                field += c;
            continue;
        }
        if (c == delimiter)
        {
            row.push_back(field);
            field.clear();
            closed = false;
        }
        else if (c == '\r' || c == '\n')
        {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
            row.push_back(field);
            rows.push_back(std::move(row));
            row.clear();
            field.clear();
            closed = false;
            pending = false;
        }
        else if (c == '"' && field.empty() && !closed)
            quoted = true;
        else
        {
            if (closed || c == '"')
                throw std::invalid_argument("Unexpected character in CSV field");
            field += c;
        }
    }
    if (quoted)
        throw std::invalid_argument("Unterminated CSV quoted field");
    if (pending || !row.empty())
    {
        row.push_back(field);
        rows.push_back(std::move(row));
    }
    return rows;
}
std::string writeCsv(const std::vector<std::vector<std::string>> &rows, char delimiter)
{
    if (delimiter == '\r' || delimiter == '\n' || delimiter == '"' || delimiter == '\0')
        throw std::invalid_argument("Invalid CSV delimiter");
    std::string out;
    for (const auto &row : rows)
    {
        for (size_t c = 0; c < row.size(); ++c)
        {
            if (c)
                out += delimiter;
            const auto &v = row[c];
            bool quote = v.find_first_of(std::string("\r\n\"") + delimiter) != std::string::npos;
            if (quote)
                out += '"';
            for (char ch : v)
            {
                out += ch;
                if (ch == '"')
                    out += '"';
            }
            if (quote)
                out += '"';
        }
        out += "\r\n";
    }
    return out;
}
} // пространство имён tvision
