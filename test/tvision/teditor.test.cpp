#define Uses_TGroup
#define Uses_TEditor
#define Uses_TText
#include <tvision/tv.h>

#include <forward_list>
#include <test.h>
#include <test_operators.h>
#include <test_charops.h>
#include <test_group.h>

struct TEditorBufferTestInput
{
    TStringView initialText;
    TStringView textToInsert;
};

struct TEditorBufferTestOutput
{
    TStringView bufferText;
    int lineEndingType;
};

class DrawableTestGroup : public TestGroup
{
public:
    DrawableTestGroup(const TRect &bounds, std::forward_list<TEvent> eventQueue = {}) noexcept :
        TestGroup(std::move(eventQueue))
    {
        changeBounds(bounds);
    }

    TColorAttr mapColor(uchar i) noexcept
    {
        return TColorAttr(i); // Используем индекс палитры как атрибут.
    }

    std::vector<std::vector<TStringView>> getBufferText()
    {
        std::vector<std::vector<TStringView>> text(
            size.y,
            std::vector<TStringView>(size.x)
        );
        for (int y = 0; y < size.y; ++y)
            for (int x = 0; x < size.x; ++x)
            {
                auto &ch = buffer[y*size.x + x].character;
                if (ch.isWideCharTrail())
                    text[y][x] = "(wide char trail)";
                else
                    text[y][x] = ch.getText();
            }
        return text;
    }

    std::vector<std::vector<int>> getBufferAttributes()
    {
        std::vector<std::vector<int>> attributes(
            size.y,
            std::vector<int>(size.x)
        );
        for (int y = 0; y < size.y; ++y)
            for (int x = 0; x < size.x; ++x)
                // Извлекаем индекс палитры из атрибута.
                attributes[y][x] = (uchar) buffer[y*size.x + x].attribute;
        return attributes;
    }
};

struct TEditorDisplayTestInput
{
    TPoint size;
    TStringView text;
    std::forward_list<TEvent> events;
    TPoint delta;
};

struct TEditorDisplayTestOutput
{
    std::vector<std::vector<TStringView>> text;
    std::vector<std::vector<int>> attributes;
    TPoint curPos;
    TPoint delta;
};

static bool operator==(const TEditorBufferTestOutput &a, const TEditorBufferTestOutput &b)
{
    return
        a.bufferText == b.bufferText &&
        a.lineEndingType == b.lineEndingType;
}

static bool operator==(const TEditorDisplayTestOutput &a, const TEditorDisplayTestOutput &b)
{
    return
        a.text == b.text &&
        a.attributes == b.attributes &&
        a.curPos == b.curPos &&
        a.delta == b.delta;
}

static std::ostream &operator<<(std::ostream &os, const TEditorBufferTestInput &self)
{
    os << "Initial Text: '" << self.initialText << '\'' << std::endl
       << "Text To Insert: '" << self.textToInsert << '\'' << std::endl;
    return os;
}

static std::ostream &operator<<(std::ostream &os, const TEditorBufferTestOutput &self)
{
    os << "Buffer Text: '" << self.bufferText << '\'' << std::endl
       << "Line Ending Type: " << self.lineEndingType << std::endl;
    return os;
}

static std::ostream &operator<<(std::ostream &os, const TEditorDisplayTestInput &self)
{
    os << "Size: " << self.size << std::endl
       << "Text: '" << self.text << '\'' << std::endl
       << "Events: " << testing::PrintToString(self.events) << std::endl
       << "Delta: " << self.delta;
    return os;
}

static std::ostream &operator<<(std::ostream &os, const TEditorDisplayTestOutput &self)
{
    os << std::endl
       << "Text: " << testing::PrintToString(self.text) << std::endl
       << "Attributes: " << testing::PrintToString(self.attributes) << std::endl
       << "CurPos: " << self.curPos << std::endl
       << "Delta: " << self.delta;
    return os;
}

TEST(TEditor, ShouldDetectAndConvertLineEndingsWhenInsertingText)
{
    static const TestCase<TEditorBufferTestInput, TEditorBufferTestOutput> testCases[] =
    {
        {   {   "a\nb\n",
                "c\n",
            },
            {   "a\nb\nc\n",
                TEditor::eolLf,
            },
        },
        {   {   "a\r\nb\r\n",
                "c\n",
            },
            {   "a\r\nb\r\nc\r\n",
                TEditor::eolCrLf,
            },
        },
        {   {   "a\nb\n",
                "c\r\n",
            },
            {   "a\nb\nc\n",
                TEditor::eolLf,
            },
        },
        {   {   "a\rb\r",
                "c\n",
            },
            {   "a\rb\rc\r",
                TEditor::eolCr,
            },
        },
        {   {   "a",
                "\nb",
            },
            {   "a\r\nb",
                TEditor::eolCrLf,
            },
        },
    };

    for (const auto &testCase : testCases)
    {
        auto initialText = testCase.input.initialText;
        auto *editor = new TEditor(TRect(), nullptr, nullptr, nullptr, 256);
        // Исходный текст помещается в конец буфера, поскольку разрыв находится
        // между курсором и оставшимся текстом.
        memcpy(
            &editor->buffer[editor->bufSize - initialText.size()],
            initialText.data(),
            initialText.size()
        );
        editor->setBufLen(initialText.size());
        editor->setCurPtr(editor->bufLen, 0);

        auto textToInsert = testCase.input.textToInsert;
        editor->insertText(textToInsert.data(), textToInsert.size(), false);

        // Теперь курсор должен находиться в конце, поэтому вставленный текст
        // можно читать с начала буфера.
        TEditorBufferTestOutput actual {
            TStringView(editor->buffer, editor->bufLen),
            editor->lineEndingType,
        };
        expectResultMatches(actual, testCase);

        TObject::destroy(editor);
    }
}

TEST(TEditor, ShouldInsertNewLinesProperly)
{
    static const TestCase<TEditor::LineEndingType, TStringView> testCases[] =
    {
        {   TEditor::eolCrLf,
            "\r\n",
        },
        {   TEditor::eolLf,
            "\n",
        },
        {   TEditor::eolCr,
            "\r",
        },
    };

    for (const auto &testCase : testCases)
    {
        auto *editor = new TEditor(TRect(), nullptr, nullptr, nullptr, 256);
        editor->lineEndingType = testCase.input;
        editor->newLine();

        // Теперь курсор должен находиться в конце, поэтому вставленный текст
        // можно читать с начала буфера.
        TStringView actual = TStringView(editor->buffer, editor->bufLen);
        expectResultMatches(actual, testCase);

        TObject::destroy(editor);
    }
}

TEST(TEditor, ShouldDrawTextAndPlaceCursorCorrectly)
{
    TestCharOps::init();
    static const TestCase<TEditorDisplayTestInput, TEditorDisplayTestOutput> testCases[] =
    {
        // Удаляем часть многобайтового символа, затем отменяем удаление.
        {   {   TPoint { 3, 1 },
                "€",
                {   messageEv(evCommand, cmTextEnd),
                    messageEv(evCommand, cmEncoding),
                    keyDownEv(kbLeft),
                    keyDownEv(kbBack),
                    keyDownEv(kbBack),
                    messageEv(evCommand, cmEncoding),
                    messageEv(evCommand, cmUndo),
                },
                TPoint { 0, 0 },
            },
            {   {{ "€", " ", " " }},
                {{   7,   6,   6 }},
                TPoint { 1, 0 },
                TPoint { 0, 0 },
            },
        },
        // При переключении в однобайтовый режим многобайтовые символы должны отображаться правильно.
        {   {   TPoint { 4, 1 },
                "€",
                {   messageEv(evCommand, cmEncoding),
                    messageEv(evCommand, cmTextEnd),
                },
                TPoint { 0, 0 },
            },
            {   {{ "Γ", "é", "¼", " " }},
                {{   6,   6,   6,   6, }},
                TPoint { 3, 0 },
                TPoint { 0, 0 },
            },
        },
        // Табуляции должны правильно отображаться при горизонтальной прокрутке.
        {   {   TPoint { 3, 1 },
                "\ta",
                {   messageEv(evCommand, cmTextEnd),
                },
                TPoint { 6, 0 },
            },
            {   {{ " ", " ", "a" }},
                {{   6,   6,   6 }},
                TPoint { 9, 0 },
                TPoint { 6, 0 },
            },
        },
        // Частично видимая выделенная табуляция должна отображаться правильно.
        {   {   TPoint { 3, 1 },
                "\ta",
                {   messageEv(evCommand, cmTextEnd),
                    keyDownEv(kbLeft),
                    keyDownEv(kbLeft, kbShift),
                },
                TPoint { 6, 0 },
            },
            {   {{ " ", " ", "a" }},
                {{   7,   7,   6 }},
                TPoint { 0, 0 },
                TPoint { 6, 0 },
            },
        },
        // Выделенный перевод строки должен отображаться правильно.
        {   {   TPoint { 3, 2 },
                "ab\ncd",
                {   keyDownEv(kbRight),
                    keyDownEv(kbRight, kbShift),
                    keyDownEv(kbRight, kbShift),
                    keyDownEv(kbRight, kbShift),
                },
                TPoint { 0, 0 },
            },
            {   {{ "a", "b", " " }, { "c", "d", " " }},
                {{   6,   7,   7 }, {   7,   6,   6 }},
                TPoint { 1, 1 },
                TPoint { 0, 0 },
            },
        },
        // Символ двойной ширины должен отображаться правильно.
        {   {   TPoint { 3, 1 },
                SMILING_FACE_WITH_SMILING_EYES_UTF8 "a",
                {},
                TPoint { 0, 0 },
            },
            {   {{ "😊", "(wide char trail)", "a" }},
                {{   6,   6,   6 }},
                TPoint { 0, 0 },
                TPoint { 0, 0 },
            },
        },
        // Частично видимый выделенный широкий символ должен отображаться правильно.
        {   {   TPoint { 2, 1 },
                SMILING_FACE_WITH_SMILING_EYES_UTF8 "a",
                {   messageEv(evCommand, cmTextEnd),
                    keyDownEv(kbLeft),
                    keyDownEv(kbLeft, kbShift),
                },
                TPoint { 1, 0 },
            },
            {   {{ " ", "a" }},
                {{   7,   6 }},
                TPoint { 0, 0 },
                TPoint { 1, 0 },
            },
        },
        // Комбинируемые символы должны отображаться правильно.
        {   {   TPoint { 2, 1 },
                "a" COMBINING_ZIGZAG_UTF8 "b" COMBINING_ZIGZAG_UTF8 "c" COMBINING_ZIGZAG_UTF8 "d" COMBINING_ZIGZAG_UTF8,
                {},
                TPoint { 1, 0 },
            },
            {   {{ "b" COMBINING_ZIGZAG_UTF8, "c" COMBINING_ZIGZAG_UTF8 }},
                {{   6,   6 }},
                TPoint { 0, 0 },
                TPoint { 1, 0 },
            },
        },
        // Ctrl+Right должен пропускать завершающие пробелы, а не останавливаться на них.
        {   {   TPoint { 7, 1 },
                "foo bar",
                {   messageEv(evCommand, cmWordRight),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", " ", "b", "a", "r" }},
                {{   6,   6,   6,   6,   6,   6,   6 }},
                TPoint { 4, 0 },
                TPoint { 0, 0 },
            },
        },
        // Ctrl+Left должен делать то же в обратном направлении.
        {   {   TPoint { 7, 1 },
                "foo bar",
                {   messageEv(evCommand, cmTextEnd),
                    messageEv(evCommand, cmWordLeft),
                    messageEv(evCommand, cmWordLeft),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", " ", "b", "a", "r" }},
                {{   6,   6,   6,   6,   6,   6,   6 }},
                TPoint { 0, 0 },
                TPoint { 0, 0 },
            },
        },
        // Ctrl+Right должен доходить до знаков препинания перед следующим словом.
        {   {   TPoint { 10, 1 },
                "foo(); bar",
                {   messageEv(evCommand, cmWordRight),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", "(", ")", ";", " ", "b", "a", "r" }},
                {{   6,   6,   6,   6,   6,   6,   6,   6,   6,   6 }},
                TPoint { 3, 0 },
                TPoint { 0, 0 },
            },
        },
        // Второй Ctrl+Right должен одновременно пропускать знаки препинания и следующие пробелы.
        {   {   TPoint { 10, 1 },
                "foo(); bar",
                {   messageEv(evCommand, cmWordRight),
                    messageEv(evCommand, cmWordRight),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", "(", ")", ";", " ", "b", "a", "r" }},
                {{   6,   6,   6,   6,   6,   6,   6,   6,   6,   6 }},
                TPoint { 7, 0 },
                TPoint { 0, 0 },
            },
        },
        // Ctrl+Left должен достигать тех же двух точек в обратном направлении.
        {   {   TPoint { 10, 1 },
                "foo(); bar",
                {   messageEv(evCommand, cmTextEnd),
                    messageEv(evCommand, cmWordLeft),
                    messageEv(evCommand, cmWordLeft),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", "(", ")", ";", " ", "b", "a", "r" }},
                {{   6,   6,   6,   6,   6,   6,   6,   6,   6,   6 }},
                TPoint { 3, 0 },
                TPoint { 0, 0 },
            },
        },
        {   {   TPoint { 10, 1 },
                "foo(); bar",
                {   messageEv(evCommand, cmTextEnd),
                    messageEv(evCommand, cmWordLeft),
                    messageEv(evCommand, cmWordLeft),
                    messageEv(evCommand, cmWordLeft),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", "(", ")", ";", " ", "b", "a", "r" }},
                {{   6,   6,   6,   6,   6,   6,   6,   6,   6,   6 }},
                TPoint { 0, 0 },
                TPoint { 0, 0 },
            },
        },
        // Ctrl+Right должен перескакивать последовательные пустые строки за один шаг.
        {   {   TPoint { 3, 4 },
                "foo\n\n\nbar",
                {   messageEv(evCommand, cmWordRight),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o" }, { " ", " ", " " }, { " ", " ", " " }, { "b", "a", "r" }},
                {{   6,   6,   6 }, {   6,   6,   6 }, {   6,   6,   6 }, {   6,   6,   6 }},
                TPoint { 0, 3 },
                TPoint { 0, 0 },
            },
        },
        // Ctrl+Left должен делать то же в обратном направлении.
        {   {   TPoint { 3, 4 },
                "foo\n\n\nbar",
                {   messageEv(evCommand, cmTextEnd),
                    messageEv(evCommand, cmWordLeft),
                    messageEv(evCommand, cmWordLeft),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o" }, { " ", " ", " " }, { " ", " ", " " }, { "b", "a", "r" }},
                {{   6,   6,   6 }, {   6,   6,   6 }, {   6,   6,   6 }, {   6,   6,   6 }},
                TPoint { 0, 0 },
                TPoint { 0, 0 },
            },
        },
        // Ctrl+Del должен за один шаг удалять подряд идущие знаки препинания и следующие пробелы.
        {   {   TPoint { 10, 1 },
                "foo(); bar",
                {   messageEv(evCommand, cmWordRight),
                    messageEv(evCommand, cmDelWord),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", "b", "a", "r", " ", " ", " ", " " }},
                {{   6,   6,   6,   6,   6,   6,   6,   6,   6,   6 }},
                TPoint { 3, 0 },
                TPoint { 0, 0 },
            },
        },
        // Ctrl+Backspace должен симметрично делать это влево.
        {   {   TPoint { 7, 1 },
                "foo bar",
                {   messageEv(evCommand, cmTextEnd),
                    messageEv(evCommand, cmDelWordLeft),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", " ", " ", " ", " " }},
                {{   6,   6,   6,   6,   6,   6,   6 }},
                TPoint { 4, 0 },
                TPoint { 0, 0 },
            },
        },
        // Двойной щелчок внутри слова выделяет ровно это слово.
        {   {   TPoint { 9, 1 },
                "foo   bar",
                {   mouseDownEv({ 1, 0 }, meDoubleClick),
                    mouseUpEv({ 1, 0 }),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", " ", " ", " ", "b", "a", "r" }},
                {{   7,   7,   7,   6,   6,   6,   6,   6,   6 }},
                TPoint { 3, 0 },
                TPoint { 0, 0 },
            },
        },
        // Двойной щелчок в последовательности пробелов выделяет только эту последовательность.
        {   {   TPoint { 9, 1 },
                "foo   bar",
                {   mouseDownEv({ 4, 0 }, meDoubleClick),
                    mouseUpEv({ 4, 0 }),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", " ", " ", " ", "b", "a", "r" }},
                {{   6,   6,   6,   7,   7,   7,   6,   6,   6 }},
                TPoint { 6, 0 },
                TPoint { 0, 0 },
            },
        },
        // Двойной щелчок по знаку препинания выделяет только подряд идущие знаки.
        {   {   TPoint { 9, 1 },
                "foo();bar",
                {   mouseDownEv({ 4, 0 }, meDoubleClick),
                    mouseUpEv({ 4, 0 }),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", "(", ")", ";", "b", "a", "r" }},
                {{   6,   6,   6,   7,   7,   7,   6,   6,   6 }},
                TPoint { 6, 0 },
                TPoint { 0, 0 },
            },
        },
        // Двойной щелчок по завершающим пробелам не должен выделять текст за переводом строки.
        {   {   TPoint { 4, 2 },
                "foo \n bar",
                {   mouseDownEv({ 3, 0 }, meDoubleClick),
                    mouseUpEv({ 3, 0 }),
                },
                TPoint { 0, 0 },
            },
            {   {{ "f", "o", "o", " " }, { " ", "b", "a", "r" }},
                {{   6,   6,   6,   7 }, {   6,   6,   6,   6 }},
                TPoint { 4, 0 },
                TPoint { 0, 0 },
            },
        },
    };

    for (const auto &testCase : testCases)
    {
        TRect bounds({0, 0}, testCase.input.size);
        auto text = testCase.input.text;
        auto delta = testCase.input.delta;
        auto *editor = new TEditor(bounds, nullptr, nullptr, nullptr, 256);
        auto *group = new DrawableTestGroup(bounds, testCase.input.events);
        group->setState(sfExposed, true);
        group->getBuffer();
        group->insert(editor);

        editor->insertText(text.data(), text.size(), false);
        editor->setCurPtr(0, 0);
        TEvent inputEvent;
        group->getEvent(inputEvent);
        while (inputEvent.what != evNothing)
        {
            editor->handleEvent(inputEvent);
            group->getEvent(inputEvent);
        }
        editor->scrollTo(delta.x, delta.y);

        TEditorDisplayTestOutput actual {
            group->getBufferText(),
            group->getBufferAttributes(),
            editor->curPos,
            editor->delta,
        };
        expectResultMatches(actual, testCase);

        TObject::destroy(group);
    }
}
