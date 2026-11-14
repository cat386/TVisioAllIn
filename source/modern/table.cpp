#define Uses_THardwareInfo
#include <algorithm>
#include <cstring>
#include <limits>
#include <tvision/modernwidgets.h>
#include <tvision/tv.h>
#include <tvision/util.h>
namespace tvision
{
TModernView::TModernView(const TRect &r) : TView(r)
{
    options |= ofSelectable | ofFirstClick;
    eventMask |= evMouseWheel;
}
TPalette &TModernView::getPalette() const
{
    static TPalette p("\x06\x07\x03\x08\x01", 5);
    return p;
}
TColorAttr TModernView::color(unsigned role)
{
    if (!custom_)
        return mapColor((uchar)(role + 1));
    switch (role)
    {
    case 1:
        return colors_.selected;
    case 2:
        return colors_.accent;
    case 3:
        return colors_.disabled;
    case 4:
        return colors_.background;
    default:
        return colors_.normal;
    }
}
void TModernView::notify(ushort command)
{
    if (owner)
        message(owner, evBroadcast, command, this);
}
void TModernView::line(int y, const std::string &text, TColorAttr attr)
{
    if (size.x <= 0 || y < 0 || y >= size.y)
        return;
    TDrawBuffer b;
    b.moveChar(0, ' ', attr, size.x);
    b.moveStr(0, text.c_str(), attr, size.x);
    writeLine(0, y, size.x, 1, b);
}
namespace
{
std::string fitText(const std::string &text, int width, Align align)
{
    if (width <= 0)
        return "";
    size_t length = 0, w = 0;
    TText::scroll(text.c_str(), width, False, length, w);
    std::string out = text.substr(0, length);
    int pad = width - (int)w;
    if (align == Align::Right)
        out.insert(0, (size_t)pad, ' ');
    else if (align == Align::Center)
    {
        out.insert(0, (size_t)(pad / 2), ' ');
        out.append((size_t)(pad - pad / 2), ' ');
    }
    else
        out.append((size_t)pad, ' ');
    return out;
}
} // namespace
TTable::TTable(const TRect &r, std::shared_ptr<TableModel> m, std::vector<TableColumn> c)
    : TModernView(r), model_(std::move(m)), columns(std::move(c))
{
    normalize();
}
void TTable::normalize()
{
    size_t count = model_ ? model_->columnCount() : 0;
    while (columns.size() < count)
        columns.emplace_back();
    for (auto &c : columns)
    {
        c.minWidth = std::max(1, c.minWidth);
        c.maxWidth = std::max(c.minWidth, c.maxWidth);
        c.width = std::max(c.minWidth, std::min(c.width, c.maxWidth));
    }
    row_ = model_ && model_->rowCount() ? std::min(row_, model_->rowCount() - 1) : 0;
    column_ = count ? std::min(column_, count - 1) : 0;
    if (count && !columns[column_].visible)
        for (size_t c = 0; c < count; ++c)
            if (columns[c].visible)
            {
                column_ = c;
                break;
            }
    frozenColumns = std::min(frozenColumns, count);
    left_ = std::max(left_, frozenColumns);
    if (count)
        left_ = std::min(left_, count - 1);
    ensureVisible();
}
void TTable::setModel(std::shared_ptr<TableModel> m)
{
    cancelEdit();
    model_ = std::move(m);
    row_ = column_ = top_ = left_ = 0;
    selected_.clear();
    normalize();
    drawView();
}
std::vector<TTable::Placement> TTable::layout() const
{
    std::vector<Placement> out;
    int x = rowNumbers ? 7 : 0;
    size_t n = model_ ? std::min(columns.size(), model_->columnCount()) : 0;
    auto add = [&](size_t c)
    {
        if (!columns[c].visible || x >= size.x)
            return;
        int w = std::max(1, std::min(columns[c].width, (int)size.x - x));
        out.push_back({c, x, w});
        x += w + (grid ? 1 : 0);
    };
    for (size_t c = 0; c < std::min(frozenColumns, n); ++c)
        add(c);
    for (size_t c = std::max(left_, frozenColumns); c < n && x < size.x; ++c)
        add(c);
    return out;
}
void TTable::ensureVisible()
{
    size_t rows = model_ ? model_->rowCount() : 0;
    int height = std::max(1, (int)size.y - (headerVisible ? 1 : 0));
    if (!rows)
    {
        top_ = 0;
        return;
    }
    if (row_ < top_)
        top_ = row_;
    if (row_ - top_ >= (size_t)height)
        top_ = row_ - (size_t)height + 1;
    if (column_ >= frozenColumns)
    {
        if (column_ < left_)
            left_ = column_;
        auto cols = layout();
        bool found = false;
        for (const auto &p : cols)
            found |= p.column == column_;
        if (!found)
            left_ = column_;
    }
}
void TTable::setCursor(size_t r, size_t c)
{
    if (editing_)
        cancelEdit();
    row_ = r;
    column_ = c;
    normalize();
    drawView();
    notify(cmTableCursorMoved);
}
void TTable::moved(size_t r, size_t c, bool extend)
{
    size_t old = row_;
    setCursor(r, c);
    if (extend && multiSelect)
    {
        if (selected_.empty())
            anchor_ = old;
        size_t first = std::min(anchor_, row_), last = std::max(anchor_, row_);
        selected_.clear();
        for (size_t i = first; i <= last; ++i)
        {
            selected_.insert(i);
            if (i == last)
                break;
        }
        drawView();
    }
    else
        anchor_ = row_;
}
bool TTable::sortBy(size_t c, bool asc)
{
    if (!model_ || c >= columns.size() || !columns[c].sortable || !model_->sort(c, asc))
        return false;
    sortColumn_ = c;
    ascending_ = asc;
    selected_.clear();
    cancelEdit();
    normalize();
    drawView();
    return true;
}
bool TTable::editCell(size_t r, size_t c, const std::string &s)
{
    if (!model_ || !model_->editable(r, c) || !model_->setCell(r, c, s))
        return false;
    drawView();
    notify(cmTableCellEdited);
    return true;
}
bool TTable::beginEdit()
{
    if (!model_ || !model_->editable(row_, column_))
        return false;
    editing_ = true;
    edit_ = model_->cell(row_, column_);
    drawView();
    return true;
}
bool TTable::commitEdit()
{
    if (!editing_)
        return true;
    if (!editCell(row_, column_, edit_))
        return false;
    editing_ = false;
    edit_.clear();
    drawView();
    return true;
}
void TTable::cancelEdit()
{
    editing_ = false;
    edit_.clear();
    drawView();
}
void TTable::resizeColumn(size_t c, int delta)
{
    if (c >= columns.size())
        return;
    auto &v = columns[c];
    int64_t n = (int64_t)v.width + delta;
    v.width = int(std::max<int64_t>(v.minWidth, std::min<int64_t>(v.maxWidth, n)));
    drawView();
}
void TTable::toggleRow(size_t r)
{
    if (!multiSelect || !model_ || r >= model_->rowCount())
        return;
    if (!selected_.erase(r))
        selected_.insert(r);
    drawView();
}
void TTable::clearSelection()
{
    selected_.clear();
    drawView();
}
std::string TTable::copySelection() const
{
    if (!model_ || !model_->rowCount() || !model_->columnCount())
        return "";
    if (selected_.empty())
        return model_->cell(row_, column_);
    std::string out;
    for (size_t r : selected_)
    {
        if (r >= model_->rowCount())
            continue;
        if (!out.empty())
            out += '\n';
        bool first = true;
        for (size_t c = 0; c < std::min(columns.size(), model_->columnCount()); ++c)
            if (columns[c].visible)
            {
                if (!first)
                    out += '\t';
                first = false;
                out += model_->cell(r, c);
            }
    }
    return out;
}
std::vector<std::string> TTable::visibleLines() const
{
    std::vector<std::string> out;
    auto positions = layout();
    for (int y = 0; y < size.y; ++y)
    {
        bool head = headerVisible && y == 0;
        size_t r = top_ + (size_t)(y - (headerVisible ? 1 : 0));
        std::string text;
        if (rowNumbers)
            text = head ? "     # "
                        : fitText(model_ && r < model_->rowCount() ? std::to_string(r + 1) : "", 6,
                                  Align::Right) +
                              " ";
        for (const auto &p : positions)
        {
            std::string cell;
            if (model_ && (head || r < model_->rowCount()))
                cell = head ? (columns[p.column].title.empty() ? model_->header(p.column)
                                                               : columns[p.column].title)
                            : (editing_ && r == row_ && p.column == column_
                                   ? edit_
                                   : model_->cell(r, p.column));
            if (head && p.column == sortColumn_)
                cell += ascending_ ? " ^" : " v";
            text += fitText(cell, p.width, head ? Align::Left : columns[p.column].align);
            if (grid)
                text += '|';
        }
        out.push_back(std::move(text));
    }
    return out;
}
TColorAttr TTable::rowColor(size_t row, bool selected, bool header)
{
    return color(header ? 2 : selected ? 1 : zebra && (row & 1) ? 4 : 0);
}
void TTable::draw()
{
    auto text = visibleLines();
    for (size_t y = 0; y < text.size(); ++y)
    {
        bool head = headerVisible && y == 0;
        size_t r = top_ + y - (headerVisible ? 1 : 0);
        line((int)y, text[y], rowColor(r, !head && (r == row_ || selected_.count(r)), head));
    }
}
void TTable::getData(void *record)
{
    uint64_t value = row_;
    if (record)
        std::memcpy(record, &value, sizeof(value));
}
void TTable::setData(void *record)
{
    if (record)
    {
        uint64_t value;
        std::memcpy(&value, record, sizeof(value));
        setCursor((size_t)std::min<uint64_t>(value, std::numeric_limits<size_t>::max()), column_);
    }
}
void TTable::changeBounds(const TRect &r)
{
    TView::changeBounds(r);
    normalize();
}
void TTable::handleEvent(TEvent &ev)
{
    TView::handleEvent(ev);
    if (!model_)
        return;
    if (ev.what == evMouseDown)
    {
        TPoint p = makeLocal(ev.mouse.where);
        auto cols = layout();
        for (const auto &c : cols)
            if (p.x >= c.x && p.x < c.x + c.width)
            {
                if (headerVisible && p.y == 0)
                    sortBy(c.column, sortColumn_ == c.column ? !ascending_ : true);
                else if (p.y >= (headerVisible ? 1 : 0))
                {
                    setCursor(top_ + (size_t)(p.y - (headerVisible ? 1 : 0)), c.column);
                    if (ev.mouse.eventFlags & meDoubleClick)
                        beginEdit();
                }
                break;
            }
        clearEvent(ev);
        return;
    }
    if (ev.what == evMouseWheel)
    {
        int d = (ev.mouse.wheel & mwUp) ? -3 : 3;
        setCursor(d < 0 ? (row_ > 2 ? row_ - 3 : 0) : row_ + 3, column_);
        clearEvent(ev);
        return;
    }
    if (ev.what != evKeyDown)
        return;
    ushort k = ev.keyDown.keyCode;
    if (editing_)
    {
        if (k == kbEsc)
            cancelEdit();
        else if (k == kbEnter)
            commitEdit();
        else if (k == kbBack)
        {
            if (!edit_.empty())
                edit_.erase(edit_.size() - TText::prev(edit_.c_str(), edit_.size()));
            drawView();
        }
        else if (ev.keyDown.textLength && edit_.size() + ev.keyDown.textLength <= 65536)
        {
            edit_.append(ev.keyDown.text, ev.keyDown.textLength);
            drawView();
        }
        else
            return;
        clearEvent(ev);
        return;
    }
    size_t r = row_, c = column_, count = model_->columnCount();
    bool ctrl = (ev.keyDown.controlKeyState & kbCtrlShift) != 0;
    switch (k)
    {
    case kbUp:
        if (r)
            --r;
        break;
    case kbDown:
        if (r < std::numeric_limits<size_t>::max())
            ++r;
        break;
    case kbPgUp:
        r = r > (size_t)std::max(1, (int)size.y - 1) ? r - (size_t)std::max(1, (int)size.y - 1) : 0;
        break;
    case kbPgDn:
        r += std::min<size_t>((size_t)std::max(1, (int)size.y - 1),
                              std::numeric_limits<size_t>::max() - r);
        break;
    case kbLeft:
        if (c)
        {
            do
            {
                --c;
            } while (c && !columns[c].visible);
        }
        break;
    case kbRight:
        if (c + 1 < count)
        {
            do
            {
                ++c;
            } while (c + 1 < count && !columns[c].visible);
        }
        break;
    case kbHome:
        c = 0;
        break;
    case kbEnd:
        c = count ? count - 1 : 0;
        break;
    case kbCtrlHome:
        r = 0;
        c = 0;
        break;
    case kbCtrlEnd:
        r = model_->rowCount() ? model_->rowCount() - 1 : 0;
        break;
    case kbCtrlLeft:
        resizeColumn(c, -1);
        clearEvent(ev);
        return;
    case kbCtrlRight:
        resizeColumn(c, 1);
        clearEvent(ev);
        return;
    case kbF2:
        beginEdit();
        clearEvent(ev);
        return;
    case kbCtrlIns:
    case kbCtrlC:
        THardwareInfo::setClipboardText(copySelection().c_str());
        clearEvent(ev);
        return;
    case kbEnter:
        notify(cmListItemSelected);
        return;
    default:
        if (k == kbCtrlA && multiSelect)
        {
            selected_.clear();
            for (size_t i = 0; i < model_->rowCount(); ++i)
                selected_.insert(i);
            drawView();
            clearEvent(ev);
            return;
        }
        if (ev.keyDown.charScan.charCode == ' ' && multiSelect)
        {
            toggleRow(row_);
            clearEvent(ev);
            return;
        }
        if (!ctrl && ev.keyDown.textLength && beginEdit())
        {
            edit_.assign(ev.keyDown.text, ev.keyDown.textLength);
            drawView();
            clearEvent(ev);
        }
        return;
    }
    moved(r, c, (ev.keyDown.controlKeyState & kbShift) != 0);
    clearEvent(ev);
}
} // namespace tvision
