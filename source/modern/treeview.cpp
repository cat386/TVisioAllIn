#include <algorithm>
#include <stdexcept>
#include <tvision/modernwidgets.h>
#include <unordered_set>
namespace tvision
{
TTreeView::TTreeView(const TRect &r, std::vector<std::shared_ptr<TreeNode>> nodes)
    : TModernView(r), roots(std::move(nodes))
{
    refresh();
}
void TTreeView::ensureVisible()
{
    if (visible_.empty())
    {
        cursor_ = top_ = 0;
        return;
    }
    cursor_ = std::min(cursor_, visible_.size() - 1);
    if (cursor_ < top_)
        top_ = cursor_;
    size_t height = (size_t)std::max(1, (int)size.y);
    if (cursor_ - top_ >= height)
        top_ = cursor_ - height + 1;
}
void TTreeView::refresh()
{
    TreeNode *old = current();
    visible_.clear();
    std::vector<Row> pending;
    std::unordered_set<TreeNode *> seen;
    for (size_t i = roots.size(); i > 0; --i)
        if (roots[i - 1])
            pending.push_back({roots[i - 1], {i - 1}});
    while (!pending.empty())
    {
        Row row = std::move(pending.back());
        pending.pop_back();
        if (row.path.size() > 200 || !seen.insert(row.node.get()).second)
            continue;
        visible_.push_back(row);
        if (row.node->expanded)
            for (size_t i = row.node->children.size(); i > 0; --i)
                if (row.node->children[i - 1])
                {
                    auto p = row.path;
                    p.push_back(i - 1);
                    pending.push_back({row.node->children[i - 1], std::move(p)});
                }
    }
    if (old)
        for (size_t i = 0; i < visible_.size(); ++i)
            if (visible_[i].node.get() == old)
            {
                cursor_ = i;
                break;
            }
    ensureVisible();
    drawView();
}
void TTreeView::changed()
{
    ensureVisible();
    drawView();
    notify(cmTreeCursorMoved);
}
void TTreeView::setCursor(size_t i)
{
    cursor_ = i;
    changed();
}
TreeNode *TTreeView::node(const TreePath &path) const
{
    if (path.empty())
        return nullptr;
    const auto *children = &roots;
    TreeNode *n = nullptr;
    for (size_t i : path)
    {
        if (i >= children->size() || !(*children)[i])
            return nullptr;
        n = (*children)[i].get();
        children = &n->children;
    }
    return n;
}
TreeNode *TTreeView::current() const
{
    return cursor_ < visible_.size() ? visible_[cursor_].node.get() : nullptr;
}
TreePath TTreeView::currentPath() const
{
    return cursor_ < visible_.size() ? visible_[cursor_].path : TreePath{};
}
bool TTreeView::expand(const TreePath &p, bool expanded)
{
    auto *n = node(p);
    if (!n)
        return false;
    if (expanded && n->lazy)
    {
        if (!loader)
        {
            error_ = "No tree loader configured";
            return false;
        }
        try
        {
            auto children = loader(*n);
            n->children = std::move(children);
            n->lazy = false;
            if (n->check != CheckState::Partial)
            {
                std::vector<TreeNode *> pending;
                for (auto &c : n->children)
                    if (c)
                        pending.push_back(c.get());
                std::unordered_set<TreeNode *> seen;
                while (!pending.empty())
                {
                    auto *c = pending.back();
                    pending.pop_back();
                    if (!seen.insert(c).second)
                        continue;
                    c->check = n->check;
                    for (auto &d : c->children)
                        if (d)
                            pending.push_back(d.get());
                }
            }
        }
        catch (const std::exception &e)
        {
            error_ = e.what();
            return false;
        }
    }
    error_.clear();
    n->expanded = expanded;
    refresh();
    notify(cmTreeToggled);
    return true;
}
bool TTreeView::setCheck(const TreePath &path, CheckState value)
{
    auto *n = node(path);
    if (!n || !n->checkable)
        return false;
    std::vector<TreeNode *> todo{n};
    std::unordered_set<TreeNode *> seen;
    while (!todo.empty())
    {
        auto *v = todo.back();
        todo.pop_back();
        if (!seen.insert(v).second)
            continue;
        v->check = value;
        for (auto &c : v->children)
            if (c)
                todo.push_back(c.get());
    }
    TreePath parent = path;
    while (parent.size() > 1)
    {
        parent.pop_back();
        auto *v = node(parent);
        bool yes = false, no = false;
        for (auto &c : v->children)
            if (c)
            {
                yes |= c->check != CheckState::Unchecked;
                no |= c->check != CheckState::Checked;
            }
        v->check = yes && no ? CheckState::Partial
                   : yes     ? CheckState::Checked
                             : CheckState::Unchecked;
    }
    drawView();
    notify(cmTreeChecked);
    return true;
}
bool TTreeView::quickFind(const std::string &text)
{
    if (text.empty() || visible_.empty())
        return false;
    for (size_t k = 1; k <= visible_.size(); ++k)
    {
        size_t i = (cursor_ + k) % visible_.size();
        const auto &label = visible_[i].node->text;
        if (label.size() >= text.size() &&
            TText::equalsIgnoreCase(TStringView(label.data(), text.size()), text.c_str()))
        {
            setCursor(i);
            return true;
        }
    }
    return false;
}
std::vector<std::string> TTreeView::visibleLines() const
{
    std::vector<std::string> out;
    for (int y = 0; y < size.y; ++y)
    {
        size_t i = top_ + (size_t)y;
        if (i >= visible_.size())
        {
            out.emplace_back();
            continue;
        }
        const auto &row = visible_[i];
        auto &n = *row.node;
        std::string s((row.path.size() - 1) * 2, ' ');
        s += n.children.empty() && !n.lazy ? "  " : n.expanded ? "v " : "> ";
        if (n.checkable)
            s += n.check == CheckState::Checked   ? "[x] "
                 : n.check == CheckState::Partial ? "[-] "
                                                  : "[ ] ";
        s += n.text;
        out.push_back(std::move(s));
    }
    return out;
}
void TTreeView::draw()
{
    auto lines = visibleLines();
    for (size_t y = 0; y < lines.size(); ++y)
        line((int)y, lines[y], color(top_ + y == cursor_ ? 1 : 0));
}
void TTreeView::handleEvent(TEvent &ev)
{
    TView::handleEvent(ev);
    if (ev.what == evMouseDown)
    {
        TPoint p = makeLocal(ev.mouse.where);
        if (p.y >= 0)
        {
            setCursor(top_ + (size_t)p.y);
            auto *n = current();
            if (n)
            {
                int indent = int(currentPath().size() - 1) * 2;
                if (p.x < indent + 2 || (ev.mouse.eventFlags & meDoubleClick))
                    expand(currentPath(), !n->expanded);
                else if (n->checkable && p.x < indent + 6)
                    setCheck(currentPath(), n->check == CheckState::Checked ? CheckState::Unchecked
                                                                            : CheckState::Checked);
            }
        }
        clearEvent(ev);
        return;
    }
    if (ev.what == evMouseWheel)
    {
        if (ev.mouse.wheel & mwUp)
            setCursor(cursor_ > 2 ? cursor_ - 3 : 0);
        else
            setCursor(cursor_ + 3);
        clearEvent(ev);
        return;
    }
    if (ev.what != evKeyDown || !current())
        return;
    auto path = currentPath();
    auto *n = current();
    ushort key = ev.keyDown.keyCode;
    switch (key)
    {
    case kbUp:
        if (cursor_)
            setCursor(cursor_ - 1);
        break;
    case kbDown:
        setCursor(cursor_ + 1);
        break;
    case kbPgUp:
        setCursor(cursor_ > (size_t)std::max(1, (int)size.y)
                      ? cursor_ - (size_t)std::max(1, (int)size.y)
                      : 0);
        break;
    case kbPgDn:
        setCursor(cursor_ + (size_t)std::max(1, (int)size.y));
        break;
    case kbHome:
        setCursor(0);
        break;
    case kbEnd:
        setCursor(visible_.empty() ? 0 : visible_.size() - 1);
        break;
    case kbRight:
        if (!n->expanded)
            expand(path);
        else if (!n->children.empty())
            setCursor(cursor_ + 1);
        break;
    case kbLeft:
        if (n->expanded)
            expand(path, false);
        else if (path.size() > 1)
        {
            path.pop_back();
            for (size_t i = 0; i < visible_.size(); ++i)
                if (visible_[i].path == path)
                {
                    setCursor(i);
                    break;
                }
        }
        break;
    case kbEnter:
        notify(cmListItemSelected);
        return;
    case kbBack:
        if (!search_.empty())
            search_.erase(search_.size() - TText::prev(search_.c_str(), search_.size()));
        quickFind(search_);
        break;
    default:
        if (ev.keyDown.charScan.charCode == ' ' && n->checkable)
            setCheck(path,
                     n->check == CheckState::Checked ? CheckState::Unchecked : CheckState::Checked);
        else if (ev.keyDown.textLength)
        {
            search_.append(ev.keyDown.text, ev.keyDown.textLength);
            if (!quickFind(search_))
            {
                search_.assign(ev.keyDown.text, ev.keyDown.textLength);
                quickFind(search_);
            }
        }
        else
            return;
    }
    clearEvent(ev);
}
} // namespace tvision
