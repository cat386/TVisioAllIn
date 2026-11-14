#define Uses_TButton
#define Uses_TProgram
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>
#include <tvision/modernwidgets.h>
#include <tvision/util.h>
namespace tvision
{
TProgressBar::TProgressBar(const TRect &r, int64_t maximum) : TModernView(r)
{
    options &= ~ofSelectable;
    setRange(0, maximum);
}
void TProgressBar::setRange(int64_t lo, int64_t hi)
{
    min_ = lo;
    max_ = std::max(lo, hi);
    value_ = std::max(min_, std::min(value_, max_));
    drawView();
}
void TProgressBar::setValue(int64_t v)
{
    int64_t next = std::max(min_, std::min(v, max_));
    if (next != value_)
    {
        value_ = next;
        drawView();
        notify(cmProgressChanged);
    }
}
void TProgressBar::step(int64_t delta)
{
    if (delta > 0 && value_ > std::numeric_limits<int64_t>::max() - delta)
        setValue(max_);
    else if (delta < 0 && value_ < std::numeric_limits<int64_t>::min() - delta)
        setValue(min_);
    else
        setValue(value_ + delta);
}
double TProgressBar::fraction() const
{
    if (max_ == min_)
        return 1;
    return double(((long double)value_ - (long double)min_) /
                  ((long double)max_ - (long double)min_));
}
unsigned TProgressBar::percent() const
{
    return value_ >= max_ ? 100u : std::min(99u, (unsigned)(fraction() * 100));
}
std::string TProgressBar::text() const
{
    if (caption.empty())
        return indeterminate ? "..." : std::to_string(percent()) + "%";
    std::string out = caption;
    const std::pair<const char *, std::string> pairs[] = {{"{value}", std::to_string(value_)},
                                                          {"{min}", std::to_string(min_)},
                                                          {"{max}", std::to_string(max_)},
                                                          {"{percent}", std::to_string(percent())}};
    for (const auto &p : pairs)
    {
        size_t i = 0;
        while ((i = out.find(p.first, i)) != std::string::npos)
        {
            out.replace(i, std::strlen(p.first), p.second);
            i += p.second.size();
        }
    }
    return out;
}
void TProgressBar::pulse()
{
    ++phase_;
    drawView();
}
void TProgressBar::draw()
{
    if (size.x <= 0 || size.y <= 0)
        return;
    int filled = int(fraction() * size.x);
    int eighth = int(fraction() * size.x * 8) % 8;
    static const char *parts[] = {"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
    int block = std::max(1, (int)size.x / 4),
        start = int(phase_ % unsigned(std::max(1, (int)size.x + block))) - block;
    for (int y = 0; y < size.y; ++y)
    {
        TDrawBuffer b;
        b.moveChar(0, ' ', color(0), size.x);
        for (int x = 0; x < size.x; ++x)
        {
            bool on = indeterminate ? (x >= start && x < start + block) : x < filled;
            b.moveStr(x,
                      on                               ? "█"
                      : style == ProgressStyle::Blocks ? "░"
                                                       : " ",
                      on ? color(2) : color(0), 1);
        }
        if (!indeterminate && style == ProgressStyle::Smooth && filled < size.x && eighth)
            b.moveStr(filled, parts[eighth], color(2), 1);
        if (showText)
        {
            std::string label = text();
            int x = std::max(0, ((int)size.x - (int)TText::width(label.c_str())) / 2);
            b.moveStr(x, label.c_str(), color(1), size.x - x);
        }
        writeLine(0, y, size.x, 1, b);
    }
}
TColor256Selector::TColor256Selector(const TRect &r, unsigned v)
    : TModernView(r), value_(std::min(v, 255u))
{
}
void TColor256Selector::setValue(unsigned v)
{
    v = std::min(v, 255u);
    if (v != value_)
    {
        value_ = v;
        drawView();
        notify(cmColor256Changed);
    }
}
void TColor256Selector::getData(void *p)
{
    if (p)
        *static_cast<unsigned char *>(p) = (unsigned char)value_;
}
void TColor256Selector::setData(void *p)
{
    if (p)
        setValue(*static_cast<unsigned char *>(p));
}
void TColor256Selector::draw()
{
    for (int y = 0; y < size.y; ++y)
    {
        TDrawBuffer b;
        b.moveChar(0, ' ', color(0), size.x);
        if (y < 16)
            for (int x = 0; x < 16 && x * 4 < size.x; ++x)
            {
                unsigned idx = unsigned(y * 16 + x);
                char text[5];
                std::snprintf(text, sizeof(text), idx == value_ ? "[%02X]" : " %02X ", idx);
                b.moveStr(x * 4, text, color256(contrast256((uchar)idx), (uchar)idx),
                          std::min(4, (int)size.x - x * 4));
            }
        writeLine(0, y, size.x, 1, b);
    }
}
void TColor256Selector::handleEvent(TEvent &ev)
{
    TView::handleEvent(ev);
    if (ev.what == evMouseDown)
    {
        auto p = makeLocal(ev.mouse.where);
        if (p.x >= 0 && p.y >= 0 && p.y < 16 && p.x < 64)
            setValue(unsigned(p.y * 16 + p.x / 4));
        clearEvent(ev);
        return;
    }
    if (ev.what != evKeyDown)
        return;
    int v = int(value_);
    switch (ev.keyDown.keyCode)
    {
    case kbLeft:
        --v;
        break;
    case kbRight:
        ++v;
        break;
    case kbUp:
        v -= 16;
        break;
    case kbDown:
        v += 16;
        break;
    case kbHome:
        v = 0;
        break;
    case kbEnd:
        v = 255;
        break;
    default:
        return;
    }
    setValue(unsigned(std::max(0, std::min(255, v))));
    clearEvent(ev);
}
TColor256Dialog::TColor256Dialog(unsigned value, TStringView title)
    : TWindowInit(&TColor256Dialog::initFrame), TDialog(TRect(0, 0, 68, 21), title)
{
    options |= ofCentered;
    auto *selector = new TColor256Selector(TRect(2, 1, 66, 17), value);
    insert(selector);
    insert(new TButton(TRect(20, 18, 32, 20), "O~K~", cmOK, bfDefault));
    insert(new TButton(TRect(35, 18, 49, 20), "~C~ancel", cmCancel, bfNormal));
    setCurrent(selector, normalSelect);
}
TInputLong::TInputLong(const TRect &r, int64_t lo, int64_t hi, bool hexadecimal)
    : TInputLine(r, 32), low_(lo), high_(std::max(lo, hi)), hex_(hexadecimal)
{
    setValue(std::max<int64_t>(lo, std::min<int64_t>(0, high_)));
}
bool TInputLong::parse(const std::string &s, int64_t &result, bool hexadecimal)
{
    size_t i = 0, end = s.size();
    while (i < end && std::isspace((unsigned char)s[i]))
        ++i;
    while (end > i && std::isspace((unsigned char)s[end - 1]))
        --end;
    bool negative = false;
    if (i < end && (s[i] == '+' || s[i] == '-'))
        negative = s[i++] == '-';
    unsigned base = 10;
    if (i < end && s[i] == '$')
    {
        if (!hexadecimal)
            return false;
        base = 16;
        ++i;
    }
    if (i == end)
        return false;
    uint64_t value = 0, limit = negative ? uint64_t(std::numeric_limits<int64_t>::max()) + 1
                                         : uint64_t(std::numeric_limits<int64_t>::max());
    for (; i < end; ++i)
    {
        unsigned char c = (unsigned char)s[i];
        unsigned digit = c >= '0' && c <= '9'   ? c - '0'
                         : c >= 'a' && c <= 'f' ? c - 'a' + 10
                         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                : 99;
        if (digit >= base || value > (limit - digit) / base)
            return false;
        value = value * base + digit;
    }
    result = negative ? (value == limit ? std::numeric_limits<int64_t>::min() : -int64_t(value))
                      : int64_t(value);
    return true;
}
bool TInputLong::value(int64_t &v) const
{
    return parse(data ? data : "", v, hex_) && v >= low_ && v <= high_;
}
void TInputLong::setValue(int64_t v)
{
    v = std::max(low_, std::min(v, high_));
    std::string text;
    if (hex_)
    {
        std::ostringstream s;
        uint64_t magnitude = v < 0 ? uint64_t(-(v + 1)) + 1 : uint64_t(v);
        s << (v < 0 ? "-$" : "$") << std::uppercase << std::hex << magnitude;
        text = s.str();
    }
    else
        text = std::to_string(v);
    std::strncpy(data, text.c_str(), (size_t)maxLen);
    data[maxLen] = 0;
    selectAll(True);
    drawView();
}
void TInputLong::getData(void *p)
{
    int64_t v = 0;
    value(v);
    if (p)
        std::memcpy(p, &v, sizeof(v));
}
void TInputLong::setData(void *p)
{
    if (p)
    {
        int64_t value;
        std::memcpy(&value, p, sizeof(value));
        setValue(value);
    }
}
Boolean TInputLong::valid(ushort command)
{
    if (command == cmCancel || command == cmValid)
        return TInputLine::valid(command);
    int64_t v;
    if (!value(v))
    {
        selectAll(True);
        return False;
    }
    return TInputLine::valid(command);
}
TCombo::TCombo(const TRect &r, int length, std::vector<std::string> items, bool only)
    : TInputLine(r, length), onlyList_(only)
{
    setItems(std::move(items));
}
void TCombo::setItems(std::vector<std::string> items)
{
    items_ = std::move(items);
    current_ = 0;
    if (!items_.empty())
        selectItem(0);
}
void TCombo::selectItem(size_t i)
{
    if (items_.empty())
        return;
    current_ = std::min(i, items_.size() - 1);
    std::strncpy(data, items_[current_].c_str(), (size_t)maxLen);
    data[maxLen] = 0;
    selectAll(True);
    drawView();
}
bool TCombo::openList()
{
    if (!owner || items_.empty())
        return false;
    int width = std::min<int>(std::max(20, (int)size.x + 4), (int)owner->size.x);
    int height = std::min<int>((int)std::min<size_t>(items_.size(), 8) + 5, (int)owner->size.y);
    if (width < 12 || height < 6)
        return false;
    std::vector<std::vector<std::string>> rows;
    for (const auto &s : items_)
        rows.push_back({s});
    auto model =
        std::make_shared<VecTableModel>(std::vector<std::string>{"Value"}, std::move(rows));
    auto *dialog = new TDialog(TRect(0, 0, width, height), "Choose value");
    dialog->options |= ofCentered;
    auto *table =
        new TTable(TRect(1, 1, width - 1, height - 3), model, {TableColumn("", width - 2)});
    table->headerVisible = false;
    table->grid = false;
    table->setCursor(current_, 0);
    dialog->insert(table);
    dialog->insert(new TButton(TRect(2, height - 3, 12, height - 1), "O~K~", cmOK, bfDefault));
    dialog->setCurrent(table, normalSelect);
    ushort result = owner->execView(dialog);
    size_t selected = table->row();
    dialog->shutDown();
    delete dialog;
    if (result == cmOK)
    {
        selectItem(selected);
        return true;
    }
    return false;
}
void TCombo::setData(void *record)
{
    TInputLine::setData(record);
    for (size_t i = 0; i < items_.size(); ++i)
        if (items_[i] == data)
        {
            current_ = i;
            break;
        }
}
void TCombo::handleEvent(TEvent &ev)
{
    if (ev.what == evKeyDown && (state & sfFocused) && !items_.empty())
    {
        ushort k = ev.keyDown.keyCode;
        if (k == kbF4 || (k == kbDown && (ev.keyDown.controlKeyState & kbAltShift)))
        {
            openList();
            clearEvent(ev);
            return;
        }
        if (k == kbDown || k == kbUp)
        {
            selectItem(k == kbDown ? (current_ + 1) % items_.size()
                                   : (current_ + items_.size() - 1) % items_.size());
            clearEvent(ev);
            return;
        }
        if (onlyList_ && ev.keyDown.textLength)
        {
            std::string search(ev.keyDown.text, ev.keyDown.textLength);
            for (size_t step = 1; step <= items_.size(); ++step)
            {
                size_t i = (current_ + step) % items_.size();
                if (items_[i].size() >= search.size() &&
                    TText::equalsIgnoreCase(TStringView(items_[i].data(), search.size()),
                                            search.c_str()))
                {
                    selectItem(i);
                    break;
                }
            }
            clearEvent(ev);
            return;
        }
        if (onlyList_ && (k == kbBack || k == kbDel || k == kbShiftIns || k == kbCtrlV))
        {
            clearEvent(ev);
            return;
        }
    }
    TInputLine::handleEvent(ev);
}
Boolean TCombo::valid(ushort command)
{
    if (command == cmCancel || command == cmValid || !onlyList_)
        return TInputLine::valid(command);
    for (const auto &s : items_)
        if (s == data)
            return True;
    return False;
}
} // namespace tvision
