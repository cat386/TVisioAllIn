#define Uses_TText
#include <tvision/tv.h>

#include <internal/dispbuff.h>
#include <internal/platform.h>

#include <test.h>
#include <test_charops.h>
#include <test_operators.h>
#include <sstream>
#include <string>
#include <vector>

namespace tvision
{

// Записывает выполненные вызовы в calls, чтобы тест мог проверить точный
// порядок операций.
class TestDisplayAdapter : public DisplayAdapter
{
public:

    TPoint size;
    std::vector<std::string> calls;

    TestDisplayAdapter(TPoint aSize) noexcept :
        size(aSize)
    {
    }

    TPoint reloadScreenInfo() noexcept override
    {
        return size;
    }

    void writeCell( TPoint pos, TStringView text,
                    TColorAttr attr, bool doubleWidth ) noexcept override
    {
        std::ostringstream oss;
        oss << "writeCell(" << pos << ", \"" << text << "\", "
            << attr << ", " << (doubleWidth ? "true" : "false") << ")";
        calls.push_back(oss.str());
    }

    void setCaretPosition(TPoint pos) noexcept override
    {
        std::ostringstream oss;
        oss << "setCaretPosition(" << pos << ")";
        calls.push_back(oss.str());
    }

    void setCaretSize(int aSize) noexcept override
    {
        calls.push_back("setCaretSize(" + std::to_string(aSize) + ")");
    }

    void clearScreen() noexcept override
    {
        calls.push_back("clearScreen()");
    }

    void flush() noexcept override
    {
        calls.push_back("flush()");
    }
};

struct DisplayBufferFlushTestInput
{
    TPoint size;
    TStringView text;
};

using DisplayBufferFlushTestOutput = std::vector<std::string>;

static std::ostream &operator<<(std::ostream &os, const DisplayBufferFlushTestInput &self)
{
    os << "Size: " << self.size << std::endl
       << "Text: '" << self.text << '\'';
    return os;
}

TEST(DisplayBuffer, ShouldRenderProperlyOnFlushScreen)
{
    TestCharOps::init();
    static const TestCase<DisplayBufferFlushTestInput, DisplayBufferFlushTestOutput> testCases[] =
    {
        // При выводе широкого символа курсор должен сместиться на два столбца.
        {   {   TPoint {4, 1},
                "[" SMILING_FACE_WITH_SMILING_EYES_UTF8 "]",
            },
            {   "writeCell({0, 0}, \"[\", {Default, Default}, false)",
                "writeCell({1, 0}, \"" SMILING_FACE_WITH_SMILING_EYES_UTF8 "\", {Default, Default}, true)",
                "writeCell({3, 0}, \"]\", {Default, Default}, false)",
                "flush()",
            },
        },
    };

    for (const auto &testCase : testCases)
    {
        auto &input = testCase.input;

        TestDisplayAdapter display(input.size);
        DisplayBuffer buf(0);
        buf.reloadScreenInfo(display);
        // Сначала обновляем экран, чтобы размер курсора установился и не
        // появился среди вызовов ниже.
        buf.flushScreen(display);

        std::vector<TScreenCell> cells(input.size.x);
        TText::drawStr({cells.data(), cells.size()}, input.text);
        buf.screenWrite(0, 0, cells.data(), (int) cells.size());

        // Для теста важны только вызовы второго flushScreen.
        display.calls.clear();
        buf.flushScreen(display);

        expectResultMatches(display.calls, testCase);
    }
}

} // пространство имён tvision
