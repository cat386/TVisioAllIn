#ifndef TVISION_MODERNWIDGETS_H
#define TVISION_MODERNWIDGETS_H
#define Uses_TView
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TInputLine
#define Uses_TText
#define Uses_TDialog
#include <cstdint>
#include <functional>
#include <set>
#include <tvision/datamodel.h>
#include <tvision/themes.h>
#include <tvision/tv.h>
namespace tvision
{
const ushort cmTableCursorMoved = 0x7001, cmTableCellEdited = 0x7002, cmTreeCursorMoved = 0x7003,
             cmTreeToggled = 0x7004, cmTreeChecked = 0x7005, cmColor256Changed = 0x7006,
             cmProgressChanged = 0x7007;
struct WidgetColors
{
    TColorAttr normal = 0x07, selected = 0x1F, accent = 0x0E, disabled = 0x08, background = 0x00;
};
class TModernView : public TView
{
    bool custom_{false};
    WidgetColors colors_;

  protected:
    TColorAttr color(unsigned role);
    void notify(ushort command);
    void line(int y, const std::string &text, TColorAttr attr);

  public:
    explicit TModernView(const TRect &bounds);
    TPalette &getPalette() const override;
    void setColors(const WidgetColors &colors)
    {
        colors_ = colors;
        custom_ = true;
        drawView();
    }
    void usePalette()
    {
        custom_ = false;
        drawView();
    }
};
enum class Align
{
    Left,
    Center,
    Right
};
struct TableColumn
{
    std::string title;
    int width = 16, minWidth = 3, maxWidth = 200;
    Align align = Align::Left;
    bool visible = true, sortable = true;
    TableColumn(std::string title = "", int width = 16, Align align = Align::Left)
        : title(std::move(title)), width(width), align(align)
    {
    }
};
class TTable : public TModernView
{
    std::shared_ptr<TableModel> model_;
    size_t row_ = 0, column_ = 0, top_ = 0, left_ = 0, anchor_ = 0, sortColumn_ = size_t(-1);
    bool ascending_ = true, editing_ = false;
    std::string edit_;
    std::set<size_t> selected_;
    struct Placement
    {
        size_t column;
        int x, width;
    };
    std::vector<Placement> layout() const;
    void normalize();
    void ensureVisible();
    void moved(size_t row, size_t column, bool extend);

  protected:
    virtual TColorAttr rowColor(size_t row, bool selected, bool header);

  public:
    std::vector<TableColumn> columns;
    bool headerVisible = true, grid = true, rowNumbers = false, zebra = true, multiSelect = false;
    size_t frozenColumns = 0;
    TTable(const TRect &bounds, std::shared_ptr<TableModel> model,
           std::vector<TableColumn> columns = {});
    void setModel(std::shared_ptr<TableModel> model);
    std::shared_ptr<TableModel> model() const
    {
        return model_;
    }
    size_t row() const
    {
        return row_;
    }
    size_t column() const
    {
        return column_;
    }
    size_t topRow() const
    {
        return top_;
    }
    void setCursor(size_t row, size_t column);
    bool sortBy(size_t column, bool ascending);
    bool editCell(size_t row, size_t column, const std::string &value);
    bool beginEdit();
    bool commitEdit();
    void cancelEdit();
    bool isEditing() const
    {
        return editing_;
    }
    void resizeColumn(size_t column, int delta);
    const std::set<size_t> &selectedRows() const
    {
        return selected_;
    }
    void toggleRow(size_t row);
    void clearSelection();
    std::string copySelection() const;
    std::vector<std::string> visibleLines() const;
    void draw() override;
    void handleEvent(TEvent &event) override;
    void changeBounds(const TRect &bounds) override;
    ushort dataSize() override
    {
        return sizeof(uint64_t);
    }
    void getData(void *record) override;
    void setData(void *record) override;
};
enum class CheckState
{
    Unchecked,
    Checked,
    Partial
};
struct TreeNode
{
    std::string text, data;
    bool expanded = false, lazy = false, checkable = false;
    CheckState check = CheckState::Unchecked;
    std::vector<std::shared_ptr<TreeNode>> children;
    explicit TreeNode(std::string text = "", std::vector<std::shared_ptr<TreeNode>> children = {})
        : text(std::move(text)), children(std::move(children))
    {
    }
};
using TreePath = std::vector<size_t>;
class TTreeView : public TModernView
{
    struct Row
    {
        std::shared_ptr<TreeNode> node;
        TreePath path;
    };
    std::vector<Row> visible_;
    size_t cursor_ = 0, top_ = 0;
    std::string search_, error_;
    void ensureVisible();
    void changed();

  public:
    std::vector<std::shared_ptr<TreeNode>> roots;
    std::function<std::vector<std::shared_ptr<TreeNode>>(TreeNode &)> loader;
    TTreeView(const TRect &bounds, std::vector<std::shared_ptr<TreeNode>> roots = {});
    void refresh();
    size_t visibleCount() const
    {
        return visible_.size();
    }
    size_t cursor() const
    {
        return cursor_;
    }
    void setCursor(size_t index);
    TreeNode *node(const TreePath &path) const;
    TreeNode *current() const;
    TreePath currentPath() const;
    bool expand(const TreePath &path, bool expanded = true);
    bool setCheck(const TreePath &path, CheckState value);
    bool quickFind(const std::string &text);
    const std::string &error() const
    {
        return error_;
    }
    std::vector<std::string> visibleLines() const;
    void draw() override;
    void handleEvent(TEvent &event) override;
};
enum class ProgressStyle
{
    Smooth,
    Blocks
};
class TProgressBar : public TModernView
{
    int64_t min_ = 0, max_ = 100, value_ = 0;
    uint64_t phase_ = 0;

  public:
    ProgressStyle style = ProgressStyle::Smooth;
    bool showText = true, indeterminate = false;
    std::string caption;
    TProgressBar(const TRect &bounds, int64_t max = 100);
    void setRange(int64_t minimum, int64_t maximum);
    void setValue(int64_t value);
    void step(int64_t delta = 1);
    int64_t value() const
    {
        return value_;
    }
    int64_t minimum() const
    {
        return min_;
    }
    int64_t maximum() const
    {
        return max_;
    }
    double fraction() const;
    unsigned percent() const;
    bool isDone() const
    {
        return !indeterminate && value_ >= max_;
    }
    std::string text() const;
    void pulse();
    void draw() override;
};
class TColor256Selector : public TModernView
{
    unsigned value_ = 0;

  public:
    explicit TColor256Selector(const TRect &bounds, unsigned value = 0);
    unsigned value() const
    {
        return value_;
    }
    void setValue(unsigned value);
    ushort dataSize() override
    {
        return sizeof(unsigned char);
    }
    void getData(void *record) override;
    void setData(void *record) override;
    void draw() override;
    void handleEvent(TEvent &event) override;
};
class TColor256Dialog : public TDialog
{
  public:
    explicit TColor256Dialog(unsigned value = 0, TStringView title = "Choose a color");
};
class TInputLong : public TInputLine
{
    int64_t low_, high_;
    bool hex_;

  public:
    TInputLong(const TRect &bounds, int64_t low, int64_t high, bool hexadecimal = false);
    static bool parse(const std::string &text, int64_t &result, bool hexadecimal = true);
    bool value(int64_t &result) const;
    void setValue(int64_t value);
    ushort dataSize() override
    {
        return sizeof(int64_t);
    }
    void getData(void *record) override;
    void setData(void *record) override;
    Boolean valid(ushort command) override;
};
class TCombo : public TInputLine
{
    std::vector<std::string> items_;
    size_t current_ = 0;
    bool onlyList_;

  public:
    TCombo(const TRect &bounds, int maxLength, std::vector<std::string> items,
           bool onlyList = true);
    const std::vector<std::string> &items() const
    {
        return items_;
    }
    void selectItem(size_t index);
    void setItems(std::vector<std::string> items);
    void setData(void *record) override;
    bool openList();
    void handleEvent(TEvent &event) override;
    Boolean valid(ushort command) override;
};
} // пространство имён tvision
#endif
