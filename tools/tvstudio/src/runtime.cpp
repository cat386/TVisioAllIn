#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <tvstudio/runtime.h>

namespace tvstudio
{
namespace
{
std::string asciiLower(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}
TRect rect(const Rect &r)
{
    return {r.x, r.y, r.x + r.w, r.y + r.h};
}
TRect formRect(const Form &f)
{
    return {f.x, f.y, f.x + f.w, f.y + f.h};
}
TSItem *items(const std::vector<std::string> &list)
{
    TSItem *next = nullptr;
    for (auto i = list.rbegin(); i != list.rend(); ++i)
        next = new TSItem(*i, next);
    return next;
}
std::string clip(const std::string &s, size_t max)
{
    size_t i = 0;
    while (i < s.size())
    {
        unsigned char c = s[i];
        size_t n = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
        if (i + n > max || i + n > s.size())
            break;
        i += n;
    }
    return s.substr(0, i);
}
class OwnedList : public TListBox
{
  public:
    using TListBox::TListBox;
    void shutDown() override
    {
        newList(nullptr);
        TListBox::shutDown();
    }
};
class RangeValidator : public TValidator
{
    int64_t low_, high_;

  public:
    RangeValidator(int64_t low, int64_t high) : low_(low), high_(high) {}
    Boolean isValid(const char *s) override
    {
        int64_t n;
        return Boolean(tvision::TInputLong::parse(s, n, false) && n >= low_ && n <= high_);
    }
    void error() override
    {
        auto text = "Введите число от " + std::to_string(low_) + " до " + std::to_string(high_);
        messageBox(text.c_str(), mfError | mfOKButton);
    }
};
TValidator *validator(const std::string &s)
{
    if (s.empty())
        return nullptr;
    auto colon = s.find(':');
    auto type = s.substr(0, colon), arg = colon == std::string::npos ? "" : s.substr(colon + 1);
    if (type == "picture")
    {
        auto *v = new TPXPictureValidator(arg, True);
        if (v->status != vsOk)
        {
            delete v;
            throw std::runtime_error("Неверная маска проверки");
        }
        return v;
    }
    if (type == "filter")
        return new TFilterValidator(arg);
    if (type == "range")
    {
        auto dots = arg.find("..");
        if (dots == std::string::npos)
            throw std::runtime_error("Диапазон задаётся как range:min..max");
        int64_t low, high;
        if (!tvision::TInputLong::parse(arg.substr(0, dots), low, false) ||
            !tvision::TInputLong::parse(arg.substr(dots + 2), high, false) || low > high)
            throw std::runtime_error("Неверный диапазон проверки");
        return new RangeValidator(low, high);
    }
    if (type == "lookup")
    {
        auto *list = new TStringCollection(8, 8);
        size_t begin = 0;
        for (size_t i = 0; i <= arg.size(); ++i)
            if (i == arg.size() || arg[i] == '|')
            {
                list->insert(newStr(arg.substr(begin, i - begin)));
                begin = i + 1;
            }
        return new TStringLookupValidator(list);
    }
    throw std::runtime_error("Неизвестная проверка ввода: " + type);
}
class LineView : public TView
{
    std::vector<Control> lines_;
    Control self_;
    int formWidth_, formHeight_;

  public:
    LineView(const Control &c, const Form &form)
        : TView(rect(c)), self_(c), formWidth_(form.w), formHeight_(form.h)
    {
        for (auto &l : form.controls)
            if (l.kind == Kind::Line)
                lines_.push_back(l);
        options &= ~ofSelectable;
    }
    TPalette &getPalette() const override
    {
        static TPalette p("\x02", 1);
        return p;
    }
    void draw() override
    {
        static const char *single[] = {" ", "│", "─", "└", "│", "│", "┌", "├",
                                       "─", "┘", "─", "┴", "┐", "┤", "┬", "┼"};
        static const char *both[] = {" ", "║", "═", "╚", "║", "║", "╔", "╠",
                                     "═", "╝", "═", "╩", "╗", "╣", "╦", "╬"};
        static const char *horizontal[] = {" ", "│", "═", "╘", "│", "│", "╒", "╞",
                                           "═", "╛", "═", "╧", "╕", "╡", "╤", "╪"};
        static const char *vertical[] = {" ", "║", "─", "╙", "║", "║", "╓", "╟",
                                         "─", "╜", "─", "╨", "╖", "╢", "╥", "╫"};
        for (int y = 0; y < size.y; ++y)
        {
            TDrawBuffer b;
            b.moveChar(0, ' ', mapColor(1), size.x);
            for (int x = 0; x < size.x; ++x)
            {
                int px = self_.x + x, py = self_.y + y, mask = 0;
                bool hd = false, vd = false;
                for (auto &line : lines_)
                {
                    if (!line.contains(px, py))
                        continue;
                    if (line.vertical)
                    {
                        if (py > line.y || line.h == 1)
                            mask |= 1;
                        if (py < line.y + line.h - 1 || line.h == 1)
                            mask |= 4;
                        vd |= line.doubleLine;
                    }
                    else
                    {
                        if (px > line.x || line.w == 1)
                            mask |= 8;
                        if (px < line.x + line.w - 1 || line.w == 1)
                            mask |= 2;
                        hd |= line.doubleLine;
                    }
                }
                if (self_.toFrame)
                {
                    if (px == 0 || px == formWidth_ - 1)
                        mask |= 1 | 4;
                    if (py == 0 || py == formHeight_ - 1)
                        mask |= 2 | 8;
                }
                auto table = hd ? (vd ? both : horizontal) : (vd ? vertical : single);
                b.moveStr(x, table[mask], mapColor(1), 1);
            }
            writeLine(0, y, size.x, 1, b);
        }
    }
};
class BranchTree : public tvision::TTreeView
{
    bool branches_;

  public:
    BranchTree(const TRect &r, std::vector<std::shared_ptr<tvision::TreeNode>> nodes, bool branches)
        : TTreeView(r, std::move(nodes)), branches_(branches)
    {
    }
    void draw() override
    {
        if (!branches_)
        {
            TTreeView::draw();
            return;
        }
        struct Row
        {
            std::shared_ptr<tvision::TreeNode> node;
            std::vector<bool> last;
        };
        std::vector<Row> rows;
        std::function<void(const std::vector<std::shared_ptr<tvision::TreeNode>> &,
                           std::vector<bool>)>
            visit = [&](const auto &nodes, std::vector<bool> last)
        {
            if (last.size() > 200)
                return;
            for (size_t i = 0; i < nodes.size(); ++i)
                if (nodes[i])
                {
                    auto p = last;
                    p.push_back(i + 1 == nodes.size());
                    rows.push_back({nodes[i], p});
                    if (nodes[i]->expanded)
                        visit(nodes[i]->children, p);
                }
        };
        visit(roots, {});
        for (int y = 0; y < size.y; ++y)
        {
            size_t index = topRow() + size_t(y);
            std::string text;
            if (index < rows.size())
            {
                auto &r = rows[index];
                for (size_t i = 1; i + 1 < r.last.size(); ++i)
                    text += r.last[i] ? "  " : "│ ";
                if (r.last.size() > 1)
                    text += r.last.back() ? "└─" : "├─";
                auto &n = *r.node;
                text += n.children.empty() ? "  " : n.expanded ? "v " : "> ";
                if (n.checkable)
                    text += n.check == tvision::CheckState::Checked   ? "[x] "
                            : n.check == tvision::CheckState::Partial ? "[-] "
                                                                      : "[ ] ";
                text += n.text;
            }
            line(y, text, color(index == cursor() ? 1 : 0));
        }
    }
};
TMenu *menu(const std::vector<Menu> &source, const Application &app)
{
    TMenuItem *first = nullptr, *last = nullptr;
    try
    {
        for (auto &m : source)
        {
            TMenuItem *item;
            if (m.separator)
                item = new TMenuItem(nullptr, 0, kbNoKey);
            else if (!m.items.empty())
                item = new TMenuItem(m.title, parseKey(m.key), menu(m.items, app), (ushort)m.help);
            else
                item = new TMenuItem(m.title, commandCode(app, m.command), parseKey(m.key),
                                     (ushort)m.help, m.key);
            if (last)
                last->next = item;
            else
                first = item;
            last = item;
        }
        return first ? new TMenu(*first) : new TMenu;
    }
    catch (...)
    {
        delete first;
        throw;
    }
}
} // namespace
TKey parseKey(const std::string &text)
{
    if (trim(text).empty())
        return {};
    auto value = asciiLower(trim(text));
    ushort mods = 0;
    while (true)
    {
        auto p = value.find('+');
        if (p == std::string::npos)
            break;
        auto part = value.substr(0, p);
        if (part == "ctrl" || part == "control")
            mods |= kbCtrlShift;
        else if (part == "shift")
            mods |= kbShift;
        else if (part == "alt")
            mods |= kbAltShift;
        else
            throw std::runtime_error("Неизвестный модификатор: " + part);
        value = value.substr(p + 1);
    }
    ushort code = 0;
    if (value.size() == 1)
    {
        code = (ushort)std::toupper(static_cast<unsigned char>(value[0]));
    }
    else if (value.size() >= 2 && value[0] == 'f' &&
             std::isdigit(static_cast<unsigned char>(value[1])))
    {
        static const ushort f[] = {kbF1, kbF2, kbF3, kbF4,  kbF5,  kbF6,
                                   kbF7, kbF8, kbF9, kbF10, kbF11, kbF12};
        int n = std::stoi(value.substr(1));
        if (n >= 1 && n <= 12)
            code = f[n - 1];
    }
    else
    {
        static const std::map<std::string, ushort> keys = {
            {"enter", kbEnter},     {"return", kbEnter},  {"esc", kbEsc},   {"escape", kbEsc},
            {"space", ushort(' ')}, {"tab", kbTab},       {"back", kbBack}, {"backspace", kbBack},
            {"ins", kbIns},         {"insert", kbIns},    {"del", kbDel},   {"delete", kbDel},
            {"home", kbHome},       {"end", kbEnd},       {"pgup", kbPgUp}, {"pageup", kbPgUp},
            {"pgdn", kbPgDn},       {"pagedown", kbPgDn}, {"left", kbLeft}, {"right", kbRight},
            {"up", kbUp},           {"down", kbDown}};
        auto it = keys.find(value);
        if (it != keys.end())
            code = it->second;
    }
    if (!code)
        throw std::runtime_error("Неизвестная клавиша: " + text);
    return TKey(code, mods);
}
ushort commandCode(const Application &app, const std::string &name)
{
    if (name.empty())
        return 0;
    std::string n = name;
    if (n.rfind("cmd::", 0) == 0)
        n = n.substr(5);
    for (auto &c : app.commands)
        if (c.name == n)
            return (ushort)c.value;
    auto key = asciiLower(n);
    if (key.rfind("cm::", 0) == 0)
        key = key.substr(4);
    else if (key.rfind("cm", 0) == 0)
        key = key.substr(2);
    static const std::map<std::string, ushort> builtin = {
        {"ok", cmOK},        {"cancel", cmCancel}, {"yes", cmYes},         {"no", cmNo},
        {"quit", cmQuit},    {"close", cmClose},   {"zoom", cmZoom},       {"resize", cmResize},
        {"next", cmNext},    {"prev", cmPrev},     {"cascade", cmCascade}, {"tile", cmTile},
        {"help", cmHelp},    {"menu", cmMenu},     {"copy", cmCopy},       {"cut", cmCut},
        {"paste", cmPaste},  {"undo", cmUndo},     {"clear", cmClear},     {"save", cmSave},
        {"saveas", cmSaveAs}};
    auto it = builtin.find(key);
    if (it != builtin.end())
        return it->second;
    throw std::runtime_error("Неизвестная команда: " + name);
}
TMenuBar *makeMenuBar(TRect r, const Application &app)
{
    r.b.y = r.a.y + 1;
    return new TMenuBar(r, menu(app.menu, app));
}
TStatusLine *makeStatusLine(TRect r, const Application &app)
{
    r.a.y = r.b.y - 1;
    TStatusItem *first = nullptr, *last = nullptr;
    try
    {
        for (auto &s : app.status)
        {
            auto *item = new TStatusItem(s.text.empty() ? TStringView() : TStringView(s.text),
                                         parseKey(s.key), commandCode(app, s.command));
            if (last)
                last->next = item;
            else
                first = item;
            last = item;
        }
        return new TStatusLine(r, *new TStatusDef(0, 0xFFFF, first));
    }
    catch (...)
    {
        delete first;
        throw;
    }
}
std::string editorText(TEditor &e)
{
    std::string out(e.buffer, e.curPtr);
    out.append(e.buffer + e.curPtr + e.gapLen, e.bufLen - e.curPtr);
    return out;
}
void replaceEditorText(TEditor &e, const std::string &s)
{
    e.setSelect(0, e.bufLen, False);
    if (!e.insertText(s.data(), (uint)s.size(), False))
        throw std::runtime_error("Недостаточно памяти редактора");
}
std::vector<std::shared_ptr<tvision::TreeNode>> treeNodes(const Control &c)
{
    std::vector<std::shared_ptr<tvision::TreeNode>> roots, parents;
    for (auto &raw : c.items)
    {
        size_t leading = raw.find_first_not_of(' ');
        if (leading == std::string::npos)
            continue;
        size_t depth = std::min<size_t>(leading / 2, parents.size());
        depth = std::min<size_t>(depth, 199);
        auto node = std::make_shared<tvision::TreeNode>(raw.substr(leading));
        node->checkable = c.checks;
        node->expanded = c.expanded;
        if (depth == 0)
            roots.push_back(node);
        else
            parents[depth - 1]->children.push_back(node);
        parents.resize(depth);
        parents.push_back(node);
    }
    return roots;
}
std::shared_ptr<tvision::VecTableModel> tableModel(const Control &c, const Json &data)
{
    std::vector<std::string> headers;
    for (auto &s : c.items)
    {
        auto p = s.find_last_of(':');
        headers.push_back(p == std::string::npos ? s : s.substr(0, p));
    }
    std::vector<std::vector<std::string>> rows;
    if (data.kind == Json::Kind::Array)
    {
        for (auto &r : data.array)
        {
            std::vector<std::string> row;
            for (auto &v : r.array)
                row.push_back(v.display());
            rows.push_back(std::move(row));
        }
    }
    else
        for (auto &raw : splitLines(c.text))
        {
            std::vector<std::string> row;
            size_t begin = 0;
            for (size_t i = 0; i <= raw.size(); ++i)
                if (i == raw.size() || raw[i] == '|')
                {
                    row.push_back(raw.substr(begin, i - begin));
                    begin = i + 1;
                }
            rows.push_back(std::move(row));
        }
    auto out = std::make_shared<tvision::VecTableModel>(std::move(headers), std::move(rows));
    out->setEditable(c.editable);
    return out;
}
TView *FormBinding::control(const std::string &name) const
{
    auto p = controls_.find(name);
    return p == controls_.end() ? nullptr : p->second;
}
void FormBinding::build(TWindow &owner, const Form &form, const Json &data, const Application &app)
{
    auto issues = form.validate();
    if (!issues.empty())
        throw std::runtime_error(issues.front());
    owner_ = &owner;
    form_ = form;
    controls_.clear();
    for (auto &c : form.controls)
    {
        if (c.kind == Kind::Label)
            continue;
        TRect bounds = rect(c);
        TView *view = nullptr;
        TScrollBar *scroll = nullptr;
        if (c.scrollbar && (c.kind == Kind::Memo || c.kind == Kind::List))
        {
            scroll = new TScrollBar(TRect(c.x + c.w - 1, c.y, c.x + c.w, c.y + c.h));
            owner.insert(scroll);
            --bounds.b.x;
        }
        switch (c.kind)
        {
        case Kind::Text:
            view = new TStaticText(bounds, c.text);
            break;
        case Kind::Button:
            view = new TButton(bounds, c.text, commandCode(app, c.command),
                               c.defaultButton ? bfDefault : bfNormal);
            break;
        case Kind::Input:
            view = new TInputLine(bounds, c.maxLength, validator(c.validator));
            break;
        case Kind::Checks:
            view = new TCheckBoxes(bounds, items(c.items));
            break;
        case Kind::Radios:
            view = new TRadioButtons(bounds, items(c.items));
            break;
        case Kind::List:
        {
            auto *l = new OwnedList(bounds, (ushort)c.columns, scroll);
            auto *strings = new TStringCollection((int)c.items.size(), 8);
            for (auto &s : c.items)
                strings->atInsert(strings->getCount(), newStr(s));
            l->newList(strings);
            view = l;
            break;
        }
        case Kind::Memo:
            view = new TMemo(bounds, nullptr, scroll, nullptr, 65535);
            break;
        case Kind::Combo:
            view = new tvision::TCombo(bounds, c.maxLength, c.items);
            break;
        case Kind::Scroll:
        {
            auto *s = new TScrollBar(bounds);
            s->setParams((int)std::max<int64_t>(0, std::min<int64_t>(c.value, INT_MAX)), 0,
                         (int)std::max<int64_t>(1, std::min<int64_t>(c.maximum, INT_MAX)), 10, 1);
            view = s;
            break;
        }
        case Kind::Progress:
            view = new tvision::TProgressBar(bounds, c.maximum);
            break;
        case Kind::Table:
        {
            std::vector<tvision::TableColumn> columns;
            for (auto &s : c.items)
            {
                auto p = s.find_last_of(':');
                int width = 16;
                if (p != std::string::npos)
                {
                    try
                    {
                        width = std::stoi(s.substr(p + 1));
                    }
                    catch (const std::exception &)
                    {
                    }
                }
                columns.emplace_back(p == std::string::npos ? s : s.substr(0, p),
                                     std::max(3, std::min(width, 200)));
            }
            auto *t = new tvision::TTable(bounds, tableModel(c), std::move(columns));
            t->headerVisible = c.header;
            t->grid = c.grid;
            t->rowNumbers = c.rowNumbers;
            view = t;
            break;
        }
        case Kind::Tree:
            view = new BranchTree(bounds, treeNodes(c), c.lines);
            break;
        case Kind::Line:
            view = new LineView(c, form);
            break;
        case Kind::Label:
            break;
        }
        if (view)
        {
            view->helpCtx = (ushort)c.help;
            if (c.disabled)
                view->state |= sfDisabled;
            owner.insert(view);
            controls_[c.name] = view;
            if (c.kind == Kind::Input && c.history > 0)
            {
                auto *h = new THistory(TRect(c.x + c.w - 3, c.y, c.x + c.w, c.y + 1),
                                       static_cast<TInputLine *>(view), (ushort)c.history);
                owner.insert(h);
            }
        }
    }
    for (auto &c : form.controls)
        if (c.kind == Kind::Label)
        {
            auto *view = new TLabel(rect(c), c.text, control(c.link));
            owner.insert(view);
            controls_[c.name] = view;
        }
    write(data);
    owner.selectNext(False);
}
void FormBinding::write(const Json &data)
{
    for (auto &c : form_.controls)
    {
        auto *view = control(c.name);
        if (!view)
            continue;
        const Json *given = data.find(c.name);
        if (c.kind == Kind::Input || c.kind == Kind::Combo)
        {
            auto *input = static_cast<TInputLine *>(view);
            std::string text = given ? given->display()
                               : c.kind == Kind::Combo
                                   ? (c.text.empty() && !c.items.empty() ? c.items[0] : c.text)
                                   : c.text;
            text = clip(text, (size_t)c.maxLength);
            std::vector<char> record((size_t)c.maxLength + 1);
            std::copy(text.begin(), text.end(), record.begin());
            input->setData(record.data());
        }
        else if (c.kind == Kind::Checks || c.kind == Kind::Radios)
        {
            ushort n = (ushort)(given && given->kind == Json::Kind::Number ? std::stoll(given->text)
                                                                           : c.value);
            view->setData(&n);
        }
        else if (c.kind == Kind::List)
        {
            auto *l = static_cast<TListBox *>(view);
            int64_t n =
                given && given->kind == Json::Kind::Number ? std::stoll(given->text) : c.value;
            if (l->range > 0)
                l->focusItem((short)std::max<int64_t>(0, std::min<int64_t>(n, l->range - 1)));
        }
        else if (c.kind == Kind::Memo)
        {
            auto *m = static_cast<TMemo *>(view);
            replaceEditorText(*m, given ? given->display() : c.text);
            m->modified = False;
        }
        else if (c.kind == Kind::Progress)
        {
            auto *p = static_cast<tvision::TProgressBar *>(view);
            p->setValue(given && given->kind == Json::Kind::Number ? std::stoll(given->text)
                                                                   : c.value);
        }
        else if (c.kind == Kind::Table && given)
            static_cast<tvision::TTable *>(view)->setModel(tableModel(c, *given));
    }
}
Json FormBinding::read() const
{
    Json out = Json::record();
    for (auto &c : form_.controls)
    {
        auto *view = control(c.name);
        if (!view || !c.hasData())
            continue;
        Json value;
        if (c.kind == Kind::Input || c.kind == Kind::Combo)
            value = Json::string(static_cast<TInputLine *>(view)->data);
        else if (c.kind == Kind::Checks || c.kind == Kind::Radios)
        {
            ushort n = 0;
            view->getData(&n);
            value = Json::number(std::to_string(n));
        }
        else if (c.kind == Kind::List)
            value = Json::number(std::to_string(static_cast<TListBox *>(view)->focused));
        else if (c.kind == Kind::Memo)
            value = Json::string(editorText(*static_cast<TMemo *>(view)));
        else if (c.kind == Kind::Progress)
            value =
                Json::number(std::to_string(static_cast<tvision::TProgressBar *>(view)->value()));
        else if (c.kind == Kind::Table)
        {
            value = Json::list();
            auto model = static_cast<tvision::TTable *>(view)->model();
            for (size_t r = 0; r < model->rowCount(); ++r)
            {
                Json row = Json::list();
                for (size_t col = 0; col < model->columnCount(); ++col)
                    row.array.push_back(Json::string(model->cell(r, col)));
                value.array.push_back(std::move(row));
            }
        }
        put(out, c.name, std::move(value));
    }
    return out;
}
FormDialog::FormDialog(const Form &form, const Json &data, const Application &app)
    : TWindowInit(&FormDialog::initFrame), TDialog(formRect(form), form.title)
{
    palette = form.palette == "blue"   ? dpBlueDialog
              : form.palette == "cyan" ? dpCyanDialog
                                       : dpGrayDialog;
    helpCtx = (ushort)form.help;
    if (form.centered)
        options |= ofCentered;
    try
    {
        binding.build(*this, form, data, app);
    }
    catch (...)
    {
        TWindow::shutDown();
        throw;
    }
}
void FormDialog::handleEvent(TEvent &ev)
{
    TDialog::handleEvent(ev);
    if (ev.what == evCommand && onCommand)
    {
        onCommand(ev.message.command, *this);
        clearEvent(ev);
    }
}
FormWindow::FormWindow(const Form &form, const Json &data, const Application &app)
    : TWindowInit(&FormWindow::initFrame),
      TWindow(formRect(form), form.title, form.number ? form.number : wnNoNumber)
{
    palette = form.palette == "blue"   ? dpBlueDialog
              : form.palette == "cyan" ? dpCyanDialog
                                       : dpGrayDialog;
    helpCtx = (ushort)form.help;
    flags = wfMove | wfClose | (form.resizable ? (wfGrow | wfZoom) : 0);
    if (form.centered)
        options |= ofCentered;
    try
    {
        binding.build(*this, form, data, app);
    }
    catch (...)
    {
        TWindow::shutDown();
        throw;
    }
}
TPalette &FormWindow::getPalette() const
{
    static TPalette gray(cpGrayDialog, sizeof(cpGrayDialog) - 1),
        blue(cpBlueDialog, sizeof(cpBlueDialog) - 1), cyan(cpCyanDialog, sizeof(cpCyanDialog) - 1);
    return palette == dpBlueDialog ? blue : palette == dpCyanDialog ? cyan : gray;
}
void FormWindow::close()
{
    if (state & sfModal)
        endModal(cmCancel);
    else
        TWindow::close();
}
void FormWindow::handleEvent(TEvent &ev)
{
    if ((state & sfModal) && ev.what == evCommand &&
        (ev.message.command == cmOK || ev.message.command == cmCancel))
    {
        endModal(ev.message.command);
        clearEvent(ev);
        return;
    }
    TWindow::handleEvent(ev);
    if (ev.what == evCommand && onCommand)
    {
        onCommand(ev.message.command, *this);
        clearEvent(ev);
    }
}
TWindow *makeForm(const Form &form, const Json &data, const Application &app,
                  CommandHandler handler)
{
    if (form.window)
    {
        auto *w = new FormWindow(form, data, app);
        w->onCommand = std::move(handler);
        return w;
    }
    auto *d = new FormDialog(form, data, app);
    d->onCommand = std::move(handler);
    return d;
}
Json readForm(TWindow &view)
{
    if (auto *d = dynamic_cast<FormDialog *>(&view))
        return d->binding.read();
    if (auto *w = dynamic_cast<FormWindow *>(&view))
        return w->binding.read();
    throw std::runtime_error("Окно не создано TV Studio");
}
ushort previewForm(TProgram &, const Form &form, Json &data, const Application &model)
{
    auto *view = makeForm(form, data, model);
    try
    {
        auto result = TProgram::deskTop->execView(view);
        if (result != cmCancel)
            data = readForm(*view);
        TObject::destroy(view);
        return result;
    }
    catch (...)
    {
        TObject::destroy(view);
        throw;
    }
}
} // namespace tvstudio
