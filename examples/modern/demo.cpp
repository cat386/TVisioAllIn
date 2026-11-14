#define Uses_TStatusLine
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TMenuBar
#define Uses_TDeskTop
#define Uses_TWindow
#define Uses_TStaticText
#define Uses_TFileEditor
#define Uses_TEditWindow
#define Uses_MsgBox
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <thread>
#include <tvision/modern.h>
#include <tvision/util.h>
#ifndef TV_MODERN_DEMO
#define TV_MODERN_DEMO "data_demo"
#endif
using namespace tvision;
namespace
{
const ushort cmModernAction = 0x7100, cmModernTheme = 0x7101, cmModernColor = 0x7102,
             cmModernExtra = 0x7103, cmModernSave = 0x7104;
std::string sampleJson()
{
    return R"([{"title":"The Rust Book","year":2024,"price":39.95,"available":true},{"title":"Turbo Vision","year":1994,"price":12.50,"available":false},{"title":"Unicode: 日本語","year":2026,"price":25,"available":true}])";
}
std::string readText(const std::string &path)
{
    if (path.empty())
        return sampleJson();
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open " + path);
    std::string out;
    char block[65536];
    while (file)
    {
        file.read(block, sizeof(block));
        out.append(block, (size_t)file.gcount());
        if (out.size() > 128 * 1024 * 1024)
            throw std::runtime_error("Example input limit: 128 MiB");
    }
    if (file.bad())
        throw std::runtime_error("Cannot read " + path);
    return out;
}
std::shared_ptr<VecTableModel> sampleTable()
{
    auto m =
        std::make_shared<VecTableModel>(std::vector<std::string>{"Name", "Count", "Price"},
                                        std::vector<std::vector<std::string>>{
                                            {"Widgets", "12", "19.90"},
                                            {"Tables", "2", "120"},
                                            {"Trees", "5", "42.50"},
                                            {"Unicode: Привет / 日本語", "1", "9007199254740993"}});
    m->setEditable(true);
    return m;
}
std::vector<std::shared_ptr<TreeNode>> sampleTree()
{
    auto root = std::make_shared<TreeNode>("Project");
    root->expanded = true;
    root->checkable = true;
    auto src = std::make_shared<TreeNode>("Sources");
    src->lazy = true;
    src->checkable = true;
    auto data = std::make_shared<TreeNode>(
        "Data", std::vector<std::shared_ptr<TreeNode>>{std::make_shared<TreeNode>("books.json"),
                                                       std::make_shared<TreeNode>("report.csv")});
    data->checkable = true;
    root->children = {src, data, std::make_shared<TreeNode>("README.md")};
    return {root};
}
int smoke(const std::string &mode)
{
    if (mode == "table")
    {
        size_t reads = 0;
        auto model = std::make_shared<FnTableModel>(1000000, 3,
                                                    [&](size_t r, size_t c)
                                                    {
                                                        ++reads;
                                                        return std::to_string(r + c);
                                                    });
        TTable table(TRect(0, 0, 60, 10), model);
        table.setCursor(999999, 2);
        table.visibleLines();
        if (reads > 27)
            throw std::runtime_error("Virtual table read too much");
    }
    else if (mode == "treeview")
    {
        TTreeView tree(TRect(0, 0, 50, 10), sampleTree());
        tree.loader = [](TreeNode &)
        { return std::vector<std::shared_ptr<TreeNode>>{std::make_shared<TreeNode>("main.cpp")}; };
        if (!tree.expand({0, 0}))
            throw std::runtime_error(tree.error());
    }
    else if (mode == "filemanager")
    {
        FilePanelModel model(".");
        if (!model.error().empty())
            throw std::runtime_error(model.error());
    }
    else if (mode == "json_load")
    {
        auto task = std::async(std::launch::async, [] { return parseJson(sampleJson()); });
        JsonTableModel model(task.get());
        if (model.rowCount() != 3)
            throw std::runtime_error("JSON row count");
    }
    else if (mode == "json_table" || mode == "json_parse")
    {
        auto json = parseJson(sampleJson());
        if (writeJson(json, true).empty())
            throw std::runtime_error("JSON serialization");
    }
    else if (mode == "progress")
    {
        TProgressBar bar(TRect(0, 0, 40, 1), 100);
        bar.step(50);
        if (bar.percent() != 50)
            throw std::runtime_error("Progress calculation");
    }
    else if (mode == "colors256" || mode == "themes")
    {
        for (auto t : {Theme::Classic, Theme::Gruvbox, Theme::Twilight})
            if (themeColors(t).size() != 135)
                throw std::runtime_error("Theme size");
    }
    else
    {
        auto model = sampleTable();
        if (parseCsv(writeCsv(model->rows())).size() != 4)
            throw std::runtime_error("CSV round trip");
    }
    std::cout << mode << ": smoke test passed\n";
    return 0;
}
class Demo : public TThemedApplication
{
    std::string mode_, path_;
    TTable *table_ = nullptr;
    TTreeView *tree_ = nullptr;
    TProgressBar *progress_ = nullptr;
    TDualPanel *panels_ = nullptr;
    TFileEditor *editor_ = nullptr;
    TCombo *combo_ = nullptr;
    std::future<Json> loading_;
    std::atomic<bool> cancel_{false};
    std::atomic<unsigned> percent_{0};
    std::chrono::steady_clock::time_point last_ = std::chrono::steady_clock::now();
    static TStatusLine *status(TRect r)
    {
        r.a.y = r.b.y - 1;
        return new TStatusLine(r, *new TStatusDef(0, 0xFFFF) +
                                      *new TStatusItem("~F4~ Action", kbF4, cmModernAction) +
                                      *new TStatusItem("~F5~ Save", kbF5, cmModernSave) +
                                      *new TStatusItem("~F6~ Theme", kbF6, cmModernTheme) +
                                      *new TStatusItem("~F7~ Color", kbF7, cmModernColor) +
                                      *new TStatusItem("~F8~ Extra", kbF8, cmModernExtra) +
                                      *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit));
    }
    TWindow *window(const std::string &title)
    {
        auto r = deskTop->getExtent();
        r.b.x = std::min<int>(r.b.x, 78);
        r.b.y = std::min<int>(r.b.y, 22);
        auto *w = new TWindow(r, title.c_str(), wnNoNumber);
        w->flags = wfMove | wfGrow | wfZoom;
        deskTop->insert(w);
        return w;
    }
    void openTable(std::shared_ptr<TableModel> model, const std::string &title)
    {
        auto *w = window(title);
        table_ = new TTable(TRect(1, 1, w->size.x - 1, w->size.y - 1), std::move(model));
        table_->rowNumbers = true;
        table_->frozenColumns = 1;
        table_->growMode = gfGrowHiX | gfGrowHiY;
        w->insert(table_);
    }

  public:
    Demo(std::string mode, std::string path)
        : TProgInit(&status, &TApplication::initMenuBar, &TApplication::initDeskTop),
          mode_(std::move(mode)), path_(std::move(path))
    {
        if (mode_ == "table")
        {
            auto m = std::make_shared<FnTableModel>(
                1000000, 4,
                [](size_t row, size_t col)
                {
                    return col == 0 ? "Row " + std::to_string(row + 1)
                                    : std::to_string((row + 1) * (col + 1));
                },
                [](size_t c) { return c == 0 ? "Million virtual rows" : columnLetters(c); });
            openTable(m, "Virtual table: only visible cells are requested");
        }
        else if (mode_ == "json_table")
        {
            auto m = std::make_shared<JsonTableModel>(parseJson(readText(path_)));
            m->setEditable(true);
            openTable(m, "JSON data: F2 edit, header click sorts");
        }
        else if (mode_ == "json_parse")
        {
            auto r = deskTop->getExtent();
            r.b.x = std::min<int>(78, r.b.x);
            r.b.y = std::min<int>(22, r.b.y);
            auto *w = new TEditWindow(r, path_.c_str(), wnNoNumber);
            w->flags = wfMove | wfGrow | wfZoom;
            editor_ = w->editor;
            if (path_.empty())
            {
                auto text = sampleJson();
                editor_->insertText(text.data(), (uint)text.size(), False);
                editor_->modified = False;
            }
            deskTop->insert(w);
        }
        else if (mode_ == "treeview")
        {
            auto *w = window("Tree: arrows expand, Space checks, typing searches");
            tree_ = new TTreeView(TRect(1, 1, w->size.x - 1, w->size.y - 1), sampleTree());
            tree_->growMode = gfGrowHiX | gfGrowHiY;
            tree_->loader = [](TreeNode &)
            {
                auto a = std::make_shared<TreeNode>("main.cpp"),
                     b = std::make_shared<TreeNode>("widgets.cpp");
                a->checkable = b->checkable = true;
                return std::vector<std::shared_ptr<TreeNode>>{a, b};
            };
            w->insert(tree_);
        }
        else if (mode_ == "progress" || mode_ == "json_load")
        {
            auto *w = window(mode_ == "progress" ? "Progress: F4 changes style, F8 indeterminate"
                                                 : "Background JSON: F4 cancels parsing");
            progress_ = new TProgressBar(TRect(2, 3, w->size.x - 2, 5), 100);
            progress_->caption = "{percent}% — {value}/{max}";
            w->insert(progress_);
            if (mode_ == "json_load")
            {
                std::string path = path_;
                loading_ = std::async(
                    std::launch::async,
                    [this, path]
                    {
                        std::string text;
                        if (!path.empty())
                            text = readText(path);
                        else
                        {
                            text = "[";
                            for (size_t i = 0; i < 100000; ++i)
                            {
                                if (cancel_)
                                    throw std::runtime_error("Loading cancelled");
                                if (i)
                                    text += ',';
                                text += "{\"row\":" + std::to_string(i) + ",\"name\":\"Record " +
                                        std::to_string(i) + "\"}";
                            }
                            text += ']';
                        }
                        return parseJson(
                            text,
                            [this](size_t done, size_t total)
                            {
                                percent_ = total ? unsigned((long double)done * 100 / total) : 100;
                                return !cancel_.load();
                            });
                    });
            }
        }
        else if (mode_ == "colors256")
        {
            auto *w = window("All 256 colors: arrows or mouse, F6 switches theme");
            w->insert(new TColor256Selector(TRect(2, 1, 66, 17), 109));
        }
        else if (mode_ == "filemanager")
        {
            auto *w = window("Files: Tab switches panels, Ins marks, F4 new tab, F8 closes tab");
            panels_ = new TDualPanel(TRect(1, 1, w->size.x - 1, w->size.y - 1),
                                     path_.empty() ? "." : path_, path_.empty() ? "." : path_);
            panels_->growMode = gfGrowHiX | gfGrowHiY;
            w->insert(panels_);
        }
        else
        {
            auto *w = window("Themes / data: F2 edits, F6 themes, F7 palette");
            table_ = new TTable(TRect(1, 1, w->size.x - 1, w->size.y - 6), sampleTable());
            table_->multiSelect = true;
            table_->rowNumbers = true;
            w->insert(table_);
            w->insert(new TStaticText(TRect(2, w->size.y - 5, 25, w->size.y - 4),
                                      "64-bit numeric input:"));
            w->insert(
                new TInputLong(TRect(2, w->size.y - 4, 25, w->size.y - 3), -1000000, 1000000));
            w->insert(new TStaticText(TRect(29, w->size.y - 5, 65, w->size.y - 4),
                                      "Combo: arrows / F4 list"));
            combo_ = new TCombo(TRect(29, w->size.y - 4, 65, w->size.y - 3), 100,
                                {"Classic", "Gruvbox", "Twilight", "Привет"});
            w->insert(combo_);
        }
    }
    ~Demo()
    {
        cancel_ = true;
        if (loading_.valid())
            loading_.wait();
        shutDown();
    }
    void idle() override
    {
        TThemedApplication::idle();
        auto now = std::chrono::steady_clock::now();
        if (progress_ && now - last_ >= std::chrono::milliseconds(100))
        {
            last_ = now;
            if (mode_ == "progress")
            {
                if (progress_->indeterminate)
                    progress_->pulse();
                else if (!progress_->isDone())
                    progress_->step();
            }
            else
                progress_->setValue(percent_.load());
        }
        if (loading_.valid() &&
            loading_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            try
            {
                auto data = loading_.get();
                progress_->setValue(100);
                auto model = std::make_shared<JsonTableModel>(std::move(data));
                model->setEditable(true);
                openTable(model, "JSON loaded in worker; UI updates on the main thread");
            }
            catch (const std::exception &e)
            {
                progress_->caption = e.what();
                progress_->drawView();
            }
        }
    }
    void handleEvent(TEvent &ev) override
    {
        TThemedApplication::handleEvent(ev);
        if (ev.what != evCommand)
            return;
        switch (ev.message.command)
        {
        case cmModernTheme:
            setTheme(theme() == Theme::Gruvbox   ? Theme::Classic
                     : theme() == Theme::Classic ? Theme::Twilight
                                                 : Theme::Gruvbox);
            break;
        case cmModernColor:
        {
            unsigned char chosen = 109;
            if (executeDialog(new TColor256Dialog(chosen), &chosen) == cmOK)
            {
                auto &palette = getPalette();
                for (int i : {13, 21, 29, 37, 69, 101})
                    palette[i].setForeground(TColorXTerm(chosen));
                redraw();
            }
            break;
        }
        case cmModernAction:
            if (combo_ && (combo_->state & sfFocused))
                combo_->openList();
            else if (editor_)
            {
                std::string text(editor_->buffer, editor_->curPtr);
                text.append(editor_->buffer + editor_->curPtr + editor_->gapLen,
                            editor_->bufLen - editor_->curPtr);
                try
                {
                    auto formatted = writeJson(parseJson(text), true);
                    editor_->setSelect(0, editor_->bufLen, False);
                    editor_->insertText(formatted.data(), (uint)formatted.size(), False);
                }
                catch (const JsonError &e)
                {
                    editor_->setSelect((uint)e.offset, (uint)e.offset, False);
                    messageBox(e.what(), mfError | mfOKButton);
                }
            }
            else if (panels_)
                panels_->active().newTab();
            else if (loading_.valid())
                cancel_ = true;
            else if (progress_)
            {
                progress_->style = progress_->style == ProgressStyle::Blocks
                                       ? ProgressStyle::Smooth
                                       : ProgressStyle::Blocks;
                progress_->drawView();
            }
            else if (table_)
                table_->beginEdit();
            break;
        case cmModernExtra:
            if (panels_)
                panels_->active().closeTab();
            else if (progress_)
            {
                progress_->indeterminate = !progress_->indeterminate;
                progress_->drawView();
            }
            else if (table_)
                table_->sortBy(table_->column(), true);
            break;
        case cmModernSave:
            if (editor_)
            {
                if (path_.empty())
                    messageBox("Pass a file name to enable saving.", mfInformation | mfOKButton);
                else
                    editor_->saveFile();
            }
            else if (table_)
            {
                std::vector<std::vector<std::string>> rows;
                auto model = table_->model();
                if (model->rowCount() > 100000)
                {
                    messageBox("Export example limit: 100000 rows", mfInformation | mfOKButton);
                    break;
                }
                std::ifstream existing("tvisio-export.csv", std::ios::binary);
                if (existing && messageBox("Overwrite tvisio-export.csv?",
                                           mfConfirmation | mfYesNoCancel) != cmYes)
                    break;
                existing.close();
                std::ofstream out("tvisio-export.csv", std::ios::binary);
                std::vector<std::string> head;
                for (size_t c = 0; c < model->columnCount(); ++c)
                    head.push_back(model->header(c));
                rows.push_back(std::move(head));
                for (size_t r = 0; r < model->rowCount(); ++r)
                {
                    std::vector<std::string> row;
                    for (size_t c = 0; c < model->columnCount(); ++c)
                        row.push_back(model->cell(r, c));
                    rows.push_back(std::move(row));
                }
                out << writeCsv(rows);
                out.close();
                messageBox(out ? "Saved tvisio-export.csv" : "Export failed",
                           mfInformation | mfOKButton);
            }
            break;
        default:
            return;
        }
        clearEvent(ev);
    }
};
} // namespace
int main(int argc, char **argv)
{
    try
    {
        std::string mode = TV_MODERN_DEMO;
        if (argc > 1 && std::string(argv[1]) == "--smoke")
            return smoke(mode);
        Demo app(mode, argc > 1 ? argv[1] : "");
        app.run();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
