#ifndef TVISION_FILEPANEL_H
#define TVISION_FILEPANEL_H
#define Uses_TGroup
#include <tvision/modernwidgets.h>
namespace tvision
{
const ushort cmFileOpen = 0x7010, cmDirectoryChanged = 0x7011;
enum class FileClass
{
    Directory,
    Executable,
    Source,
    Archive,
    Image,
    Media,
    Document,
    Other
};
struct PanelEntry
{
    std::string name, path;
    uint64_t size = 0;
    int64_t modified = 0;
    bool directory = false, parent = false, hidden = false, symlink = false, marked = false;
    FileClass fileClass() const;
};
bool globMatch(const std::string &pattern, const std::string &text, bool caseSensitive = false);
std::string formatFileSize(uint64_t size);
class FilePanelModel : public TableModel
{
    std::string path_, filter_, error_;
    bool hidden_ = false, reverse_ = false;
    size_t sort_ = 0;
    std::vector<PanelEntry> entries_;
    std::vector<size_t> visible_;
    void rebuild();

  public:
    explicit FilePanelModel(const std::string &path = ".");
    bool changeDirectory(const std::string &path);
    bool refresh();
    const std::string &path() const
    {
        return path_;
    }
    const std::string &error() const
    {
        return error_;
    }
    void setFilter(std::string pattern);
    void showHidden(bool value);
    size_t rowCount() const override
    {
        return visible_.size();
    }
    size_t columnCount() const override
    {
        return 4;
    }
    std::string header(size_t column) const override;
    std::string cell(size_t row, size_t column) const override;
    bool sort(size_t column, bool ascending) override;
    const PanelEntry *entry(size_t row) const;
    void toggleMark(size_t row);
    void markGlob(const std::string &pattern, bool marked = true);
    void invertMarks();
    std::vector<std::string> markedPaths() const;
};
class TFilePanel : public TTable
{
    std::vector<std::shared_ptr<FilePanelModel>> tabs_;
    size_t tab_ = 0;
    std::vector<size_t> cursors_;

  protected:
    TColorAttr rowColor(size_t row, bool selected, bool header) override;

  public:
    bool colorByType = true;
    std::array<unsigned char, 8> typeColors{{214, 142, 109, 167, 175, 108, 223, 250}};
    TFilePanel(const TRect &bounds, const std::string &path = ".");
    std::shared_ptr<FilePanelModel> files() const
    {
        return tabs_[tab_];
    }
    bool enter();
    bool goUp();
    bool changeDirectory(const std::string &path);
    void newTab(const std::string &path = "");
    bool closeTab();
    void selectTab(size_t index);
    size_t tabCount() const
    {
        return tabs_.size();
    }
    size_t activeTab() const
    {
        return tab_;
    }
    void handleEvent(TEvent &event) override;
};
class TDualPanel : public TGroup
{
    TFilePanel *left_, *right_;

  public:
    TDualPanel(const TRect &bounds, const std::string &left = ".", const std::string &right = ".");
    TFilePanel &left() const
    {
        return *left_;
    }
    TFilePanel &right() const
    {
        return *right_;
    }
    TFilePanel &active() const;
    TFilePanel &other() const;
    void switchPanel();
    void syncOther();
    void changeBounds(const TRect &bounds) override;
};
} // пространство имён tvision
#endif
