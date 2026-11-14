#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <test.h>
#include <test_group.h>
#include <tvision/modern.h>
using namespace tvision;
namespace
{
class ModernDrawGroup : public TestGroup
{
  public:
    ModernDrawGroup()
    {
        changeBounds(TRect(0, 0, 64, 20));
        setState(sfExposed, True);
        getBuffer();
    }
    TColorAttr mapColor(uchar i) noexcept
    {
        return TColorAttr(i);
    }
    std::string rowText(int y) const
    {
        std::string out;
        for (int x = 0; x < size.x; ++x)
        {
            auto &ch = buffer[y * size.x + x].character;
            if (!ch.isWideCharTrail())
            {
                auto s = ch.getText();
                out.append(s.data(), s.size());
            }
        }
        return out;
    }
};
class ModernThemeRoot : public TestGroup
{
    std::vector<TScreenCell> screen_;

  public:
    ThemePalette palette;
    explicit ModernThemeRoot(bool buffered) : screen_(64 * 20)
    {
        changeBounds(TRect(0, 0, 64, 20));
        setState(sfExposed, True);
        if (buffered)
            getBuffer();
        else
        {
            options &= ~ofBuffered;
            buffer = screen_.data(); // Как экранный буфер TProgram, принадлежащий внешнему владельцу.
        }
    }
    TPalette &getPalette() const override
    {
        return palette.palette();
    }
    TColorAttr attribute(int x, int y) const
    {
        return buffer[y * size.x + x].attribute;
    }
};
class ModernThemeProbe : public TView
{
  public:
    unsigned draws = 0;
    ModernThemeProbe() : TView(TRect(0, 0, 16, 1)) {}
    TPalette &getPalette() const override
    {
        static TPalette palette("\x0D", 1);
        return palette;
    }
    void draw() override
    {
        ++draws;
        TDrawBuffer line;
        line.moveChar(0, 'X', mapColor(1), size.x);
        writeLine(0, 0, size.x, 1, line);
    }
};
} // пространство имён
TEST(ModernThemes, RefreshRepaintsNestedCachedGroupsForEveryTheme)
{
    for (bool buffered : {true, false})
    {
        auto *root = new ModernThemeRoot(buffered);
        auto *outer = new TGroup(TRect(0, 0, 32, 10));
        auto *inner = new TGroup(TRect(0, 0, 24, 5));
        auto *probe = new ModernThemeProbe;
        inner->insert(probe);
        outer->insert(inner);
        root->insert(outer);
        refreshTheme(*root);
        auto before = root->attribute(0, 0);
        auto draws = probe->draws;
        root->palette.setTheme(Theme::Classic);
        root->redraw();
        EXPECT_EQ(root->attribute(0, 0), before); // Обычный redraw повторно использует кэшированное изображение.
        EXPECT_EQ(probe->draws, draws);
        for (auto theme : {Theme::Classic, Theme::Twilight, Theme::Gruvbox})
        {
            root->palette.setTheme(theme);
            refreshTheme(*root);
            EXPECT_EQ(root->attribute(0, 0), themeColors(theme)[12]);
            EXPECT_TRUE(probe->draws > draws);
            draws = probe->draws;
            EXPECT_TRUE((root->state & sfExposed) != 0);
        }
        root->setState(sfExposed, False);
        refreshTheme(*root);
        EXPECT_EQ(probe->draws, draws);
        EXPECT_FALSE((root->state & sfExposed) != 0);
        TObject::destroy(root);
    }
}
TEST(ModernDrawing, TableProgressAndColorGridRenderIntoGroup)
{
    auto *group = new ModernDrawGroup;
    auto model = std::make_shared<VecTableModel>(std::vector<std::string>{"Name"},
                                                 std::vector<std::vector<std::string>>{{"Alpha"}});
    auto *table = new TTable(TRect(0, 0, 30, 3), model);
    auto *progress = new TProgressBar(TRect(0, 4, 30, 5));
    auto *colors = new TColor256Selector(TRect(0, 5, 64, 20), 32);
    group->insert(table);
    group->insert(progress);
    group->insert(colors);
    progress->setValue(50);
    group->drawView();
    table->draw();
    progress->draw();
    colors->draw();
    EXPECT_TRUE(group->rowText(0).find("Name") != std::string::npos);
    EXPECT_TRUE(group->rowText(1).find("Alpha") != std::string::npos);
    EXPECT_TRUE(group->rowText(4).find("50%") != std::string::npos);
    EXPECT_TRUE(group->rowText(5).find("00") != std::string::npos);
    TObject::destroy(group);
}
namespace
{
TEvent modernKey(ushort code, const std::string &text = "", ushort mods = 0)
{
    TEvent e{};
    e.what = evKeyDown;
    e.keyDown.keyCode = code;
    e.keyDown.controlKeyState = mods;
    e.keyDown.textLength = (uchar)std::min<size_t>(4, text.size());
    std::memcpy(e.keyDown.text, text.data(), e.keyDown.textLength);
    return e;
}
std::string modernUtf8(const std::filesystem::path &p)
{
    auto s = p.u8string();
    return {reinterpret_cast<const char *>(s.data()), s.size()};
}
template <class F> bool jsonFails(F f)
{
    try
    {
        f();
        return false;
    }
    catch (const JsonError &)
    {
        return true;
    }
}
} // пространство имён
TEST(ModernData, ColumnsAndExactNumericOrder)
{
    EXPECT_EQ(columnLetters(0), "A");
    EXPECT_EQ(columnLetters(25), "Z");
    EXPECT_EQ(columnLetters(26), "AA");
    EXPECT_EQ(columnLetters(701), "ZZ");
    EXPECT_TRUE(naturalCompare("9007199254740993", "9007199254740992") > 0);
    EXPECT_TRUE(naturalCompare("-100000000000000000001", "-100000000000000000000") < 0);
    EXPECT_EQ(naturalCompare("1e2", "100.00"), 0);
    EXPECT_EQ(naturalCompare("-0", "0"), 0);
    EXPECT_TRUE(naturalCompare("2,5", "10") < 0);
    EXPECT_TRUE(naturalCompare("file2", "file10") < 0);
    EXPECT_TRUE(naturalCompare("1e2", "1x") < 0);
    EXPECT_TRUE(naturalCompare("2", "1e2") < 0);
}
TEST(ModernData, RaggedEditingAndStableSort)
{
    VecTableModel m({"Name", "Count"}, {{"a", "10"}, {"b", "2"}, {"c", "2"}, {"ragged"}});
    EXPECT_EQ(m.columnCount(), 2u);
    EXPECT_EQ(m.cell(3, 1), "");
    EXPECT_FALSE(m.setCell(3, 1, "7"));
    m.setEditable(true);
    EXPECT_TRUE(m.setCell(3, 1, "7"));
    EXPECT_FALSE(m.setCell(9, 0, "x"));
    EXPECT_TRUE(m.sort(1, true));
    EXPECT_EQ(m.cell(0, 0), "b");
    EXPECT_EQ(m.cell(1, 0), "c");
}
TEST(ModernData, CsvRoundTripQuotedDelimitersAndLines)
{
    std::vector<std::vector<std::string>> data = {{"a,b", "\"quoted\"", "two\nlines", ""},
                                                  {"Русский", "42", "x", ""}};
    EXPECT_EQ(parseCsv(writeCsv(data)), data);
    EXPECT_EQ(parseCsv("a;b\r\nc;d\r\n", ';').size(), 2u);
    bool failed = false;
    try
    {
        parseCsv("\"unterminated");
    }
    catch (const std::invalid_argument &)
    {
        failed = true;
    }
    EXPECT_TRUE(failed);
}
TEST(ModernJson, PreservesNumbersOrderAndUnicode)
{
    auto j = parseJson(
        "{\"big\":900719925474099312345,\"glyph\":\"\\uD83D\\uDE00\",\"zero\":0.10,\"x\":true}");
    EXPECT_EQ(j.object[0].first, "big");
    EXPECT_EQ(j.find("big")->text, "900719925474099312345");
    EXPECT_EQ(j.find("glyph")->text, "\xF0\x9F\x98\x80");
    EXPECT_EQ(j.find("zero")->text, "0.10");
    EXPECT_EQ(writeJson(parseJson(writeJson(j, true))), writeJson(j));
}
TEST(ModernJson, RejectsMalformedInputAndReportsPosition)
{
    for (const std::string s : {"01", "[1,]", "{\"a\":1,\"a\":2}", "\"\\uD800\"", "1e", "\"\\x\""})
        EXPECT_TRUE(jsonFails([&] { parseJson(s); }));
    EXPECT_TRUE(jsonFails([&] { parseJson(std::string("\"") + char(0xC0) + char(0x80) + "\""); }));
    try
    {
        parseJson("[\n 1,\n invalid]");
        EXPECT_TRUE(false);
    }
    catch (const JsonError &e)
    {
        EXPECT_EQ(e.line, 3u);
        EXPECT_EQ(e.column, 2u);
    }
    EXPECT_TRUE(jsonFails([&] { parseJson(std::string(202, '[') + std::string(202, ']')); }));
}
TEST(ModernJson, CancellationAndTypedTableEditing)
{
    bool cancelled = false;
    try
    {
        parseJson("[1,2]", [](size_t, size_t) { return false; });
    }
    catch (const JsonError &e)
    {
        cancelled = e.cancelled;
    }
    EXPECT_TRUE(cancelled);
    JsonTableModel m(
        parseJson("[{\"name\":\"b\",\"count\":10},{\"name\":\"a\",\"count\":2,\"flag\":true}]"));
    EXPECT_EQ(m.columnCount(), 3u);
    EXPECT_EQ(m.header(0), "name");
    m.setEditable(true);
    EXPECT_FALSE(m.setCell(0, 1, "not a number"));
    EXPECT_TRUE(m.setCell(0, 1, "9007199254740993"));
    EXPECT_TRUE(m.sort(1, true));
    EXPECT_EQ(m.cell(0, 0), "a");
    EXPECT_EQ(m.cell(1, 1), "9007199254740993");
}
TEST(ModernThemes, AllSlotsAndBiosFallback)
{
    auto g = themeColors(Theme::Gruvbox);
    EXPECT_EQ(g.size(), 135u);
    EXPECT_EQ(g[0], color256(237, 234));
    auto classic = themeColors(Theme::Classic);
    EXPECT_EQ(classic[0], TColorAttr(0x71));
    auto fallback = themeColors(Theme::Gruvbox, false);
    for (auto c : fallback)
    {
        EXPECT_TRUE(c.getForeground().isBIOS());
        EXPECT_TRUE(c.getBackground().isBIOS());
    }
    ThemePalette p;
    p.setTheme(Theme::Twilight);
    EXPECT_EQ(p.palette()[1], themeColors(Theme::Twilight)[0]);
    EXPECT_EQ(contrast256(0), 15);
    EXPECT_EQ(contrast256(15), 0);
}
TEST(ModernTable, MillionRowsOnlyReadVisibleCells)
{
    size_t calls = 0;
    auto m = std::make_shared<FnTableModel>(1000000, 2,
                                            [&](size_t r, size_t c)
                                            {
                                                ++calls;
                                                return std::to_string(r) + ":" + std::to_string(c);
                                            });
    TTable t(TRect(0, 0, 40, 8), m);
    EXPECT_EQ(calls, 0u);
    t.setCursor(999999, 1);
    auto lines = t.visibleLines();
    EXPECT_EQ(lines.size(), 8u);
    EXPECT_EQ(calls, 14u);
    EXPECT_TRUE(lines.back().find("999999") != std::string::npos);
    auto key = modernKey(kbDown);
    t.handleEvent(key);
    EXPECT_EQ(t.row(), 999999u);
}
TEST(ModernTable, EditingSelectionHiddenColumnsAndBounds)
{
    auto m = std::make_shared<VecTableModel>(
        std::vector<std::string>{"A", "B"},
        std::vector<std::vector<std::string>>{{"one", "1"}, {"two", "2"}});
    m->setEditable(true);
    TTable t(TRect(0, 0, 30, 6), m);
    t.multiSelect = true;
    t.toggleRow(0);
    t.toggleRow(1);
    EXPECT_EQ(t.copySelection(), "one\t1\ntwo\t2");
    t.columns[1].visible = false;
    EXPECT_EQ(t.copySelection(), "one\ntwo");
    t.setCursor(999, 999);
    EXPECT_EQ(t.row(), 1u);
    EXPECT_EQ(t.column(), 0u);
    EXPECT_TRUE(t.editCell(1, 0, "changed"));
    auto f2 = modernKey(kbF2);
    t.handleEvent(f2);
    EXPECT_TRUE(t.isEditing());
    auto esc = modernKey(kbEsc);
    t.handleEvent(esc);
    EXPECT_FALSE(t.isEditing());
    uint64_t row = 0;
    char unaligned[9] = {};
    row = 1;
    std::memcpy(unaligned + 1, &row, 8);
    t.setData(unaligned + 1);
    EXPECT_EQ(t.row(), 1u);
}
TEST(ModernTree, LazyLoadOnceAndTriStatePropagation)
{
    auto root = std::make_shared<TreeNode>("Root");
    root->checkable = true;
    root->lazy = true;
    TTreeView t(TRect(0, 0, 40, 8), {root});
    size_t loads = 0;
    t.loader = [&](TreeNode &)
    {
        ++loads;
        auto a = std::make_shared<TreeNode>("Alpha"), b = std::make_shared<TreeNode>("Beta");
        a->checkable = b->checkable = true;
        return std::vector<std::shared_ptr<TreeNode>>{a, b};
    };
    EXPECT_TRUE(t.setCheck({0}, CheckState::Checked));
    EXPECT_TRUE(t.expand({0}));
    EXPECT_EQ(loads, 1u);
    EXPECT_EQ(t.visibleCount(), 3u);
    EXPECT_EQ(root->children[0]->check, CheckState::Checked);
    EXPECT_TRUE(t.setCheck({0, 1}, CheckState::Unchecked));
    EXPECT_EQ(root->check, CheckState::Partial);
    EXPECT_TRUE(t.expand({0}, false));
    EXPECT_TRUE(t.expand({0}));
    EXPECT_EQ(loads, 1u);
    EXPECT_TRUE(t.quickFind("Beta"));
    EXPECT_EQ(t.current()->text, "Beta");
}
TEST(ModernTree, LoaderFailureAndInvalidPaths)
{
    auto root = std::make_shared<TreeNode>("Root");
    root->lazy = true;
    TTreeView t(TRect(0, 0, 30, 5), {root});
    EXPECT_FALSE(t.expand({0}));
    EXPECT_TRUE(root->lazy);
    EXPECT_FALSE(t.expand({99}));
    EXPECT_EQ(t.node({0, 9}), nullptr);
    t.loader = [](TreeNode &) -> std::vector<std::shared_ptr<TreeNode>>
    { throw std::runtime_error("load failed"); };
    EXPECT_FALSE(t.expand({0}));
    EXPECT_EQ(t.error(), "load failed");
}
TEST(ModernProgress, FullInt64RangeAndSaturatingSteps)
{
    TProgressBar p(TRect(0, 0, 30, 1));
    p.setRange(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
    p.setValue(std::numeric_limits<int64_t>::max() - 1);
    EXPECT_EQ(p.percent(), 99u);
    p.step(10);
    EXPECT_EQ(p.percent(), 100u);
    p.setValue(-1);
    p.step(std::numeric_limits<int64_t>::min());
    EXPECT_EQ(p.value(), std::numeric_limits<int64_t>::min());
    p.setRange(0, 10);
    p.setValue(4);
    p.caption = "{value}/{max} {percent}%";
    EXPECT_EQ(p.text(), "4/10 40%");
    p.indeterminate = true;
    EXPECT_FALSE(p.isDone());
}
TEST(ModernInputs, HexAndFullSignedRangeWithoutOverflow)
{
    int64_t v = 0;
    EXPECT_TRUE(TInputLong::parse("-$8000000000000000", v));
    EXPECT_EQ(v, std::numeric_limits<int64_t>::min());
    EXPECT_FALSE(TInputLong::parse("9223372036854775808", v));
    EXPECT_FALSE(TInputLong::parse("$FF", v, false));
    TInputLong input(TRect(0, 0, 30, 1), -50, 100, true);
    input.setValue(500);
    EXPECT_TRUE(input.value(v));
    EXPECT_EQ(v, 100);
    char data[9];
    v = -23;
    std::memcpy(data + 1, &v, 8);
    input.setData(data + 1);
    input.getData(data + 1);
    std::memcpy(&v, data + 1, 8);
    EXPECT_EQ(v, -23);
    TCombo combo(TRect(0, 0, 20, 1), 30, {"Alpha", "Beta"});
    combo.state |= sfFocused;
    auto key = modernKey(kbDown);
    combo.handleEvent(key);
    EXPECT_EQ(std::string(combo.data), "Beta");
    char restored[31] = "Alpha";
    combo.setData(restored);
    key = modernKey(kbDown);
    combo.handleEvent(key);
    EXPECT_EQ(std::string(combo.data), "Beta");
}
TEST(ModernColors, SelectorKeyboardAndDataTransfer)
{
    TColor256Selector selector(TRect(0, 0, 64, 16), 255);
    auto right = modernKey(kbRight);
    selector.handleEvent(right);
    EXPECT_EQ(selector.value(), 255u);
    auto home = modernKey(kbHome);
    selector.handleEvent(home);
    EXPECT_EQ(selector.value(), 0u);
    unsigned char color = 143;
    selector.setData(&color);
    EXPECT_EQ(selector.value(), 143u);
}
TEST(ModernColors, DialogInitiallyFocusesSelector)
{
    TColor256Dialog dialog(42);
    ASSERT_TRUE(dialog.current != nullptr);
    EXPECT_EQ(dialog.current->dataSize(), 1u);
    auto right = modernKey(kbRight);
    dialog.handleEvent(right);
    unsigned char value = 0;
    dialog.getData(&value);
    EXPECT_EQ(value, 43);
    dialog.shutDown();
}
TEST(ModernInputs, ComboPopupKeyboardSelectsItem)
{
    TEvent accept{};
    accept.what = evCommand;
    accept.message.command = cmOK;
    TestGroup group({modernKey(kbDown), accept}, accept);
    group.changeBounds(TRect(0, 0, 60, 15));
    auto *combo = new TCombo(TRect(1, 1, 30, 2), 30, {"Alpha", "Beta"});
    group.insert(combo);
    EXPECT_TRUE(combo->openList());
    EXPECT_EQ(std::string(combo->data), "Beta");
    group.shutDown();
}
TEST(ModernFiles, UnicodeGlobsAndFileClasses)
{
    EXPECT_TRUE(globMatch("?.txt", "я.txt"));
    EXPECT_TRUE(globMatch("*.RS", "code.rs"));
    EXPECT_FALSE(globMatch("a?", "a"));
    EXPECT_TRUE(globMatch("*a*b*", "012a345b678"));
    EXPECT_FALSE(globMatch("*.rs", "x.cpp"));
    PanelEntry e;
    e.name = "test.rs";
    EXPECT_EQ(e.fileClass(), FileClass::Source);
    EXPECT_EQ(formatFileSize(1024), "1.0 KiB");
}
TEST(ModernFiles, ListingFilteringMarksAndTabs)
{
    namespace fs = std::filesystem;
    auto dir = fs::temp_directory_path() /
               ("tvisio-modern-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir / "sub");
    {
        std::ofstream f(dir / "a.txt");
        f << "abc";
    }
    {
        std::ofstream f(dir / "b.rs");
        f << "source";
    }
    {
        TFilePanel panel(TRect(0, 0, 40, 10), modernUtf8(dir));
        auto m = panel.files();
        EXPECT_EQ(m->rowCount(), 4u);
        EXPECT_TRUE(m->entry(0)->parent);
        EXPECT_TRUE(m->entry(1)->directory);
        m->setFilter("*.txt");
        EXPECT_EQ(m->rowCount(), 3u);
        m->markGlob("*.txt");
        EXPECT_EQ(m->markedPaths().size(), 1u);
        EXPECT_TRUE(m->refresh());
        EXPECT_EQ(m->markedPaths().size(), 1u);
        panel.newTab(modernUtf8(dir / "sub"));
        EXPECT_EQ(panel.tabCount(), 2u);
        EXPECT_TRUE(panel.closeTab());
        EXPECT_EQ(panel.tabCount(), 1u);
        auto old = m->path();
        EXPECT_FALSE(m->changeDirectory(modernUtf8(dir / "missing")));
        EXPECT_EQ(m->path(), old);
    }
    fs::remove_all(dir);
}
