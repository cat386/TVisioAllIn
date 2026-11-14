#define Uses_TIndicator
#define Uses_TFrame
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <tvstudio/codegen.h>
#include <tvstudio/lsp.h>
#include <tvstudio/process.h>
#include <tvstudio/runtime.h>

namespace tvstudio
{
namespace
{
enum : ushort
{
    NewProject = 500,
    OpenProject,
    OpenFile,
    Save,
    SaveAll,
    NewForm,
    Properties,
    Preview,
    Generate,
    Build,
    Run,
    Test,
    Clean,
    Stop,
    ProjectTree,
    Palette,
    AppModel,
    BuildOptionsCommand,
    Find,
    Replace,
    Goto,
    Complete,
    ThemeCommand,
    About,
    Redo,
    AlignLeft,
    AlignTop,
    AlignWidth,
    TabForward,
    TabBack,
    Import,
    NextError,
    Definition,
    References,
    Clangd,
    CodeEditor,
    FormDesigner,
    NewSource,
    Help
};

bool ask(const char *title, const char *label, std::string &value)
{
    char buffer[4096] = {};
    std::copy_n(value.data(), std::min(value.size(), sizeof(buffer) - 1), buffer);
    auto *dialog = new TDialog(TRect(0, 0, 72, 9), title);
    dialog->options |= ofCentered;
    auto *input = new TInputLine(TRect(2, 3, 70, 4), sizeof(buffer) - 1);
    input->setData(buffer);
    dialog->insert(input);
    dialog->insert(new TLabel(TRect(2, 1, 70, 2), label, input));
    dialog->insert(new TButton(TRect(15, 6, 32, 8), "OK", cmOK, bfDefault));
    dialog->insert(new TButton(TRect(38, 6, 55, 8), "Отмена", cmCancel, bfNormal));
    input->select();
    auto result = TProgram::deskTop->execView(dialog);
    if (result == cmOK)
        input->getData(buffer);
    TObject::destroy(dialog);
    if (result == cmCancel)
        return false;
    value = buffer;
    return true;
}
void notice(const std::string &text, bool error = false)
{
    messageBox(text.c_str(), (error ? mfError : mfInformation) | mfOKButton);
}

// Файлами управляет IDE: встроенный редактор не должен открывать Save As при проверке диалога.
class BufferEditor : public TFileEditor
{
  public:
    using TFileEditor::TFileEditor;
    Boolean valid(ushort command) override
    {
        return command == cmValid ? isValid : True;
    }
};

// Многострочный редактор свойств сохраняет неизвестные поля формата TV Studio.
bool editDocument(const std::string &title, std::string &text)
{
    auto area = TProgram::deskTop->getExtent();
    area.grow(-2, -1);
    if (area.b.x - area.a.x < 40 || area.b.y - area.a.y < 10)
        area = TProgram::deskTop->getExtent();
    auto *dialog = new TDialog(area, title.c_str());
    dialog->options |= ofCentered;
    int w = dialog->size.x, h = dialog->size.y;
    auto *scroll = new TScrollBar(TRect(w - 2, 2, w - 1, h - 4));
    dialog->insert(scroll);
    auto *editor = new BufferEditor(TRect(2, 2, w - 2, h - 4), nullptr, scroll, nullptr, "");
    replaceEditorText(*editor, text);
    editor->modified = False;
    dialog->insert(editor);
    dialog->insert(new TButton(TRect(2, h - 3, 18, h - 1), "Применить", cmOK, bfDefault));
    dialog->insert(new TButton(TRect(20, h - 3, 34, h - 1), "Отмена", cmCancel, bfNormal));
    editor->select();
    auto result = TProgram::deskTop->execView(dialog);
    if (result == cmOK)
        text = editorText(*editor);
    editor->modified = False;
    TObject::destroy(dialog);
    return result == cmOK;
}

class SourceEditor : public BufferEditor
{
    std::vector<std::pair<std::string, uint>> undo_, redo_;
    bool restoring_ = false;

  public:
    explicit SourceEditor(TRect r, TScrollBar *h, TScrollBar *v, TIndicator *i)
        : BufferEditor(r, h, v, i, "")
    {
        autoIndent = True;
    }
    void snapshot()
    {
        if (restoring_)
            return;
        auto text = editorText(*this);
        if (undo_.empty() || undo_.back().first != text)
        {
            undo_.emplace_back(std::move(text), curPtr);
            if (undo_.size() > 64)
                undo_.erase(undo_.begin());
        }
        redo_.clear();
    }
    void restore(bool forward)
    {
        auto &from = forward ? redo_ : undo_, &to = forward ? undo_ : redo_;
        if (from.empty())
            return;
        to.emplace_back(editorText(*this), curPtr);
        auto value = std::move(from.back());
        from.pop_back();
        restoring_ = true;
        replaceEditorText(*this, value.first);
        setSelect(std::min<uint>(value.second, bufLen), std::min<uint>(value.second, bufLen),
                  False);
        modified = True;
        restoring_ = false;
        trackCursor(False);
    }
    void handleEvent(TEvent &event) override
    {
        if ((event.what == evCommand && event.message.command == cmUndo) ||
            (event.what == evKeyDown && event.keyDown.keyCode == kbCtrlZ))
        {
            restore(false);
            clearEvent(event);
            return;
        }
        if (event.what == evCommand && event.message.command == Redo)
        {
            restore(true);
            clearEvent(event);
            return;
        }
        auto before = editorText(*this);
        auto cursor = curPtr;
        TFileEditor::handleEvent(event);
        if (editorText(*this) != before)
        {
            if (undo_.empty() || undo_.back().first != before)
                undo_.emplace_back(std::move(before), cursor);
            if (undo_.size() > 64)
                undo_.erase(undo_.begin());
            redo_.clear();
        }
    }
    // Раскраска работает поверх родного редактора, сохраняя UTF-8, выделение и прокрутку.
    void draw() override
    {
        TFileEditor::draw();
        uint start = lineStart(lineMove(curPtr, -curPos.y + delta.y));
        static const std::set<std::string> words = {
            "alignas",  "auto",     "bool",      "break",     "case",      "catch",    "char",
            "class",    "const",    "constexpr", "continue",  "default",   "delete",   "do",
            "double",   "else",     "enum",      "explicit",  "false",     "float",    "for",
            "if",       "include",  "int",       "long",      "namespace", "new",      "noexcept",
            "nullptr",  "override", "private",   "protected", "public",    "return",   "short",
            "signed",   "sizeof",   "static",    "std",       "struct",    "switch",   "template",
            "this",     "throw",    "true",      "try",       "typedef",   "typename", "union",
            "unsigned", "using",    "virtual",   "void",      "volatile",  "while"};
        for (int row = 0; row < size.y && start < bufLen; ++row)
        {
            TDrawBuffer line;
            formatLine(line, start, delta.x, size.x, getColor(0x0201));
            uint end = lineEnd(start);
            bool comment = false;
            char quote = 0;
            for (uint p = start; p < end;)
            {
                uint next = nextChar(p);
                char c = bufChar(p);
                unsigned color = 0;
                if (comment)
                    color = 109;
                else if (quote)
                {
                    color = 111;
                    if (c == '\\')
                        next = std::min(end, nextChar(next));
                    else if (c == quote)
                        quote = 0;
                }
                else if (c == '/' && p + 1 < end && bufChar(p + 1) == '/')
                {
                    comment = true;
                    color = 109;
                }
                else if (c == '"' || c == '\'')
                {
                    quote = c;
                    color = 111;
                }
                else if (std::isdigit(static_cast<unsigned char>(c)))
                    color = 113;
                else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
                {
                    std::string word(1, c);
                    while (next < end && (std::isalnum(static_cast<unsigned char>(bufChar(next))) ||
                                          bufChar(next) == '_'))
                        word += bufChar(next++);
                    if (words.count(word))
                        color = 110;
                }
                if (color)
                {
                    int x = charPos(start, p) - delta.x, z = charPos(start, next) - delta.x;
                    for (int cell = std::max(0, x); cell < std::min(size.x, z); ++cell)
                        if (!(p < selEnd && next > selStart))
                            line.putAttribute(cell, TProgram::application->getColor(color));
                }
                p = next;
            }
            writeLine(0, row, size.x, 1, line);
            uint next = nextLine(start);
            if (next <= start)
                break;
            start = next;
        }
    }
    void go(int line, int column)
    {
        uint p = lineMove(0, std::max(0, line - 1));
        p = charPtr(p, std::max(0, column - 1));
        setSelect(p, p, False);
        trackCursor(False);
        drawView();
    }
    void complete()
    {
        auto text = editorText(*this);
        size_t start = curPtr;
        while (start && (std::isalnum(static_cast<unsigned char>(text[start - 1])) ||
                         text[start - 1] == '_'))
            --start;
        auto prefix = text.substr(start, curPtr - start);
        if (prefix.empty())
            return;
        std::set<std::string> words;
        for (size_t i = 0; i < text.size();)
        {
            if (std::isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_')
            {
                size_t p = i++;
                while (i < text.size() &&
                       (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_'))
                    ++i;
                auto word = text.substr(p, i - p);
                if (word.size() > prefix.size() && word.rfind(prefix, 0) == 0)
                    words.insert(word);
            }
            else
                ++i;
        }
        std::string choices;
        for (auto &word : words)
        {
            if (!choices.empty())
                choices += " | ";
            choices += word;
        }
        if (words.empty())
            return;
        std::string value = *words.begin();
        if (words.size() == 1 || ask("Дополнение", choices.c_str(), value))
        {
            snapshot();
            setSelect(uint(start), curPtr, False);
            insertText(value.data(), uint(value.size()), False);
        }
    }
};

class SourceWindow : public TWindow
{
    std::string titleText_;

  public:
    fs::path path;
    SourceEditor *editor;
    bool closed = false;
    SourceWindow(TRect bounds, fs::path file)
        : TWindowInit(&SourceWindow::initFrame),
          TWindow(bounds, file.empty() ? "Без имени.cpp" : utf8(file.filename()).c_str(),
                  wnNoNumber),
          titleText_(file.empty() ? "Без имени.cpp" : utf8(file.filename())), path(std::move(file))
    {
        auto *h = standardScrollBar(sbHorizontal | sbHandleKeyboard),
             *v = standardScrollBar(sbVertical | sbHandleKeyboard);
        auto *indicator = new TIndicator(TRect(2, size.y - 1, 18, size.y));
        insert(indicator);
        editor = new SourceEditor(TRect(1, 1, size.x - 1, size.y - 1), h, v, indicator);
        editor->growMode = gfGrowHiX | gfGrowHiY;
        insert(editor);
        if (fs::exists(path))
            replaceEditorText(*editor, readFile(path));
        editor->modified = False;
        editor->setSelect(0, 0, False);
        editor->trackCursor(False);
        editor->select();
    }
    const char *getTitle(short) override
    {
        return titleText_.c_str();
    }
    bool save()
    {
        if (path.empty())
        {
            std::string value = "main.cpp";
            if (!ask("Сохранить исходник", "Путь к файлу:", value) || value.empty())
                return false;
            auto target = fs::absolute(pathFromUtf8(value)).lexically_normal();
            if (fs::exists(target) &&
                messageBox("Перезаписать существующий файл?",
                           mfConfirmation | mfYesButton | mfNoButton) != cmYes)
                return false;
            writeFile(target, editorText(*editor));
            path = target;
            titleText_ = utf8(path.filename());
        }
        else
            writeFile(path, editorText(*editor));
        editor->modified = False;
        editor->update(ufUpdate);
        if (frame)
            frame->drawView();
        return true;
    }
    bool allowClose()
    {
        if (!editor->modified)
            return true;
        auto result = messageBox(("Сохранить " + titleText_ + "?").c_str(),
                                 mfConfirmation | mfYesButton | mfNoButton | mfCancelButton);
        if (result == cmCancel)
            return false;
        if (result == cmYes)
            return save();
        return true;
    }
    // Окно остаётся собственностью приложения: указатели на редакторы не висят после закрытия.
    void close() override
    {
        if (allowClose())
        {
            hide();
            closed = true;
        }
    }
};

class ProjectFiles : public tvision::TTreeView
{
  public:
    using tvision::TTreeView::TTreeView;
    std::function<void(const std::string &)> activate;
    void handleEvent(TEvent &event) override
    {
        bool open = event.what == evMouseDown && (event.mouse.eventFlags & meDoubleClick) &&
                    mouseInView(event.mouse.where);
        TTreeView::handleEvent(event);
        if (open && activate)
        {
            auto *selected = current();
            if (selected && !selected->data.empty())
            {
                auto path = selected->data;
                activate(path);
                clearEvent(event);
            }
        }
    }
};

class PanelWindow : public TWindow
{
  public:
    PanelWindow(TRect r, const char *title)
        : TWindowInit(&PanelWindow::initFrame), TWindow(r, title, wnNoNumber)
    {
    }
    void close() override
    {
        hide();
    }
};

class Designer : public FormWindow
{
    Application app_;

  public:
    FormDocument document;
    std::function<void()> changed;
    Designer(Form value, Application app)
        : TWindowInit(&Designer::initFrame), FormWindow(value, {}, app), app_(std::move(app)),
          document(std::move(value))
    {
        options |= ofPreProcess;
    }
    void rebuild()
    {
        lock();
        std::vector<TView *> children;
        for (TView *v = first(); v;)
        {
            if (v != frame)
                children.push_back(v);
            v = v->nextView();
            if (v == first())
                break;
        }
        for (auto *v : children)
        {
            remove(v);
            TObject::destroy(v);
        }
        binding.build(*this, document.form, {}, app_);
        palette = document.form.palette == "blue"   ? dpBlueDialog
                  : document.form.palette == "cyan" ? dpCyanDialog
                                                    : dpGrayDialog;
        unlock();
        drawView();
        if (changed)
            changed();
    }
    void draw() override
    {
        FormWindow::draw();
        for (auto index : document.selection)
            if (index < document.form.controls.size())
            {
                auto &c = document.form.controls[index];
                TDrawBuffer b;
                for (auto xy : {std::pair<int, int>{c.x, c.y},
                                {c.x + c.w - 1, c.y},
                                {c.x, c.y + c.h - 1},
                                {c.x + c.w - 1, c.y + c.h - 1}})
                    if (xy.first > 0 && xy.second > 0 && xy.first < size.x - 1 &&
                        xy.second < size.y - 1)
                    {
                        b.moveChar(0, '+', getColor(3), 1);
                        writeLine(xy.first, xy.second, 1, 1, b);
                    }
            }
    }
    const char *getTitle(short) override
    {
        return document.form.title.c_str();
    }
    void sizeLimits(TPoint &minimum, TPoint &maximum) override
    {
        FormWindow::sizeLimits(minimum, maximum);
        for (auto &c : document.form.controls)
        {
            minimum.x = std::max(minimum.x, c.x + c.w + 1);
            minimum.y = std::max(minimum.y, c.y + c.h + 1);
        }
    }
    void changeBounds(const TRect &r) override
    {
        FormWindow::changeBounds(r);
        if (!document.form.centered)
        {
            document.form.x = r.a.x;
            document.form.y = r.a.y;
        }
        document.form.w = size.x;
        document.form.h = size.y;
        if (changed)
            changed();
    }
    void handleEvent(TEvent &e) override
    {
        if (e.what == evMouseDown && mouseInView(e.mouse.where))
        {
            auto p = makeLocal(e.mouse.where);
            if (p.x > 0 && p.y > 0 && p.x < size.x - 1 && p.y < size.y - 1)
            {
                select();
                int hit = document.form.hit(p.x, p.y);
                bool extend = (e.mouse.controlKeyState & kbCtrlShift) != 0;
                if (!extend)
                    document.selection.clear();
                if (hit >= 0)
                {
                    auto index = size_t(hit);
                    if (extend && document.selection.count(index))
                        document.selection.erase(index);
                    else
                        document.selection.insert(index);
                }
                drawView();
                if (changed)
                    changed();
                if (hit >= 0 && !document.selection.empty())
                {
                    bool resize = (e.mouse.controlKeyState & kbShift) != 0;
                    auto origin = document.form;
                    bool checkpoint = false;
                    while (mouseEvent(e, evMouseMove))
                    {
                        auto q = makeLocal(e.mouse.where);
                        if (q == p)
                            continue;
                        if (!checkpoint)
                        {
                            document.checkpoint();
                            checkpoint = true;
                        }
                        document.form = origin;
                        // move создаёт собственный снимок; во время протягивания меняем геометрию
                        // прямо.
                        for (auto i : document.selection)
                        {
                            auto &c = document.form.controls[i], &old = origin.controls[i];
                            if (resize)
                            {
                                c.w = std::max(
                                    1, std::min(document.form.w - 1 - c.x, old.w + q.x - p.x));
                                c.h = std::max(
                                    1, std::min(document.form.h - 1 - c.y, old.h + q.y - p.y));
                            }
                            else
                            {
                                c.x = std::max(
                                    1, std::min(document.form.w - 1 - c.w, old.x + q.x - p.x));
                                c.y = std::max(
                                    1, std::min(document.form.h - 1 - c.h, old.y + q.y - p.y));
                            }
                        }
                        rebuild();
                    }
                }
                clearEvent(e);
                return;
            }
        }
        if (e.what == evKeyDown && (state & sfSelected))
        {
            ushort key = e.keyDown.keyCode;
            bool handled = true;
            if (key == kbDel)
                document.erase();
            else if (key == kbCtrlZ)
                document.undo();
            else if (key == kbCtrlC)
                document.copy();
            else if (key == kbCtrlX)
                document.cut();
            else if (key == kbCtrlV)
                document.paste();
            else if (key == kbLeft || key == kbRight || key == kbUp || key == kbDown)
                document.move((key == kbRight) - (key == kbLeft), (key == kbDown) - (key == kbUp),
                              (e.keyDown.controlKeyState & kbShift) != 0);
            else
                handled = false;
            if (handled)
            {
                rebuild();
                clearEvent(e);
                return;
            }
        }
        // В режиме проектирования кнопки и поля не выполняют команды приложения.
        if (e.what == evCommand && e.message.command != cmClose && e.message.command != cmZoom &&
            e.message.command != cmResize)
            return;
        FormWindow::handleEvent(e);
    }
    void close() override
    {
        hide();
    }
};

Application studioMenus()
{
    Application a;
    auto item = [](const char *title, ushort command, const char *key = "")
    { return Menu{title, std::to_string(command), key, 0, false, {}}; };
    a.menu = {
        {"~Ф~айл",
         "",
         "",
         0,
         false,
         {item("Новый проект...", NewProject), item("Новый исходник", NewSource, "Ctrl+N"),
          item("Открыть проект...", OpenProject, "Ctrl+O"), item("Импорт C++ / CMake...", Import),
          item("Открыть исходник...", OpenFile), item("Сохранить", Save, "F2"),
          item("Сохранить всё", SaveAll, "Ctrl+S"), item("Выход", cmQuit, "Alt+X")}},
        {"~П~равка",
         "",
         "",
         0,
         false,
         {item("Отменить", cmUndo, "Ctrl+Z"), item("Повторить", Redo),
          item("Вырезать", cmCut, "Ctrl+X"), item("Копировать", cmCopy, "Ctrl+C"),
          item("Вставить", cmPaste, "Ctrl+V"), item("Найти...", Find, "Ctrl+F"),
          item("Заменить...", Replace, "Ctrl+R"), item("Строка...", Goto, "Ctrl+G"),
          item("Дополнить слово", Complete, "Ctrl+Space")}},
        {"Ф~о~рма",
         "",
         "",
         0,
         false,
         {item("Новая форма...", NewForm), item("Компоненты... (Insert)", Palette),
          item("Свойства... (Enter)", Properties), item("Предпросмотр", Preview, "F9"),
          item("Выровнять слева", AlignLeft), item("Выровнять сверху", AlignTop),
          item("Одинаковая ширина", AlignWidth), item("Раньше в Tab-порядке", TabBack),
          item("Позже в Tab-порядке", TabForward)}},
        {"Прое~к~т",
         "",
         "",
         0,
         false,
         {item("Дерево проекта", ProjectTree, "F3"),
          item("Меню, команды, строка состояния...", AppModel), item("Генерировать C++", Generate),
          item("Настройки сборки...", BuildOptionsCommand),
          item("clangd: команда запуска...", Clangd),
          item("Определение символа", Definition, "F12"),
          item("Использования символа", References)}},
        {"~С~борка",
         "",
         "",
         0,
         false,
         {item("Собрать", Build, "F7"), item("Собрать и запустить", Run, "Ctrl+F9"),
          item("Тесты", Test), item("Очистить", Clean), item("Остановить", Stop, "Ctrl+F2"),
          item("Следующая ошибка", NextError, "F8")}},
        {"~О~кна",
         "",
         "",
         0,
         false,
         {item("Редактор кода", CodeEditor, "F4"), item("Дизайнер форм", FormDesigner, "Shift+F4"),
          item("Следующее", cmNext, "F6"), item("Мозаика", cmTile), item("Каскад", cmCascade),
          item("Тема", ThemeCommand)}},
        {"~?~",
         "",
         "",
         0,
         false,
         {item("Работа в TV Studio", Help, "F1"), item("О программе", About)}}};
    // Имена локальных команд разрешаются общим построителем меню.
    for (unsigned c = NewProject; c <= Help; ++c)
        a.commands.push_back({std::to_string(c), "", "", c});
    for (ushort c : {cmQuit, cmUndo, cmCut, cmCopy, cmPaste, cmNext, cmTile, cmCascade})
        a.commands.push_back({std::to_string(c), "", "", c});
    a.status = {{"~F1~ Помощь", "F1", std::to_string(Help)},
                {"~F2~ Сохранить", "F2", std::to_string(Save)},
                {"~F7~ Сборка", "F7", std::to_string(Build)},
                {"~F9~ Форма", "F9", std::to_string(Preview)},
                {"~Alt+X~ Выход", "Alt+X", std::to_string(cmQuit)}};
    return a;
}

class Studio : public tvision::TThemedApplication
{
    std::unique_ptr<Project> project_;
    std::map<std::string, Designer *> forms_;
    std::vector<SourceWindow *> sources_;
    TWindow *projectWindow_ = nullptr, *messagesWindow_ = nullptr, *inspectorWindow_ = nullptr;
    tvision::TTable *inspector_ = nullptr;
    Designer *inspected_ = nullptr;
    tvision::TTreeView *tree_ = nullptr;
    TFileEditor *messages_ = nullptr;
    BuildRunner builder_;
    LspClient lsp_;
    bool lspAttempted_ = false;
    std::set<fs::path> lspFiles_;
    std::chrono::steady_clock::time_point lspSync_{};
    bool runAfter_ = false, building_ = false;
    std::string outputTail_;
    std::vector<Diagnostic> diagnostics_;
    size_t diagnostic_ = 0;
    fs::path library_;
    static TMenuBar *menu(TRect r)
    {
        return makeMenuBar(r, studioMenus());
    }
    static TStatusLine *status(TRect r)
    {
        return makeStatusLine(r, studioMenus());
    }
    Designer *designer()
    {
        for (auto &p : forms_)
            if (p.second->state & sfSelected)
                return p.second;
        return nullptr;
    }
    SourceWindow *source()
    {
        for (auto *p : sources_)
            if (p->state & sfSelected)
                return p;
        return nullptr;
    }
    TRect editorArea() const
    {
        auto r = deskTop->getExtent();
        r.b.y = std::max(8, r.b.y - 6);
        if (r.b.x > 90)
            r.b.x -= 27;
        return r;
    }
    void log(const std::string &text)
    {
        if (!messages_)
        {
            auto *selected = deskTop->current;
            auto r = deskTop->getExtent();
            r.a.y = std::max(1, r.b.y - 6);
            messagesWindow_ = new PanelWindow(r, "Сообщения сборки — F8: ошибка");
            auto *v = messagesWindow_->standardScrollBar(sbVertical | sbHandleKeyboard);
            messages_ = new TFileEditor(TRect(1, 1, r.b.x - 1, 5), nullptr, v, nullptr, "");
            messages_->growMode = gfGrowHiX | gfGrowHiY;
            messagesWindow_->insert(messages_);
            deskTop->insert(messagesWindow_);
            if (selected)
                selected->select();
        }
        std::string value = editorText(*messages_);
        value += text;
        if (value.size() > 2 * 1024 * 1024)
            value.erase(0, value.find('\n', value.size() - 1024 * 1024) + 1);
        replaceEditorText(*messages_, value);
        messages_->modified = False;
        messages_->setSelect(messages_->bufLen, messages_->bufLen, False);
        messages_->trackCursor(False);
        messagesWindow_->show();
    }
    void sync()
    {
        if (project_)
            for (auto &p : forms_)
            {
                int i = project_->formIndex(p.first);
                if (i >= 0)
                    project_->forms[size_t(i)] = p.second->document.form;
            }
    }
    bool saveAll()
    {
        for (auto *s : sources_)
            if (s->editor->modified)
                if (!s->save())
                    return false;
        if (project_)
        {
            sync();
            project_->openFiles.clear();
            for (auto *s : sources_)
                if (!s->closed && !s->path.empty() && within(s->path, project_->directory))
                    project_->openFiles.push_back(utf8(fs::relative(s->path, project_->directory)));
            project_->save();
            for (auto &p : forms_)
                p.second->document.markSaved();
        }
        return true;
    }
    bool closeProject()
    {
        bool dirty = false;
        for (auto &p : forms_)
            dirty |= p.second->document.dirty();
        for (auto *s : sources_)
            dirty |= bool(s->editor->modified);
        if (dirty)
        {
            auto r = messageBox("Сохранить изменения проекта?",
                                mfConfirmation | mfYesButton | mfNoButton | mfCancelButton);
            if (r == cmCancel)
                return false;
            if (r == cmYes && !saveAll())
                return false;
        }
        builder_.stop();
        lsp_.stop();
        lspAttempted_ = false;
        lspFiles_.clear();
        runAfter_ = false;
        for (auto &p : forms_)
            TObject::destroy(p.second);
        forms_.clear();
        for (auto *p : sources_)
        {
            p->editor->modified = False;
            TObject::destroy(p);
        }
        sources_.clear();
        if (projectWindow_)
        {
            TObject::destroy(projectWindow_);
            projectWindow_ = nullptr;
            tree_ = nullptr;
        }
        if (inspectorWindow_)
        {
            TObject::destroy(inspectorWindow_);
            inspectorWindow_ = nullptr;
            inspector_ = nullptr;
        }
        inspected_ = nullptr;
        project_.reset();
        return true;
    }
    void load(const fs::path &path)
    {
        auto p = std::make_unique<Project>(Project::load(path));
        if (!closeProject())
            return;
        project_ = std::move(p);
        tree();
        if (!project_->forms.empty())
            openForm(project_->forms.front().name);
        bool opened = false;
        for (auto &name : project_->openFiles)
        {
            auto file = project_->directory / pathFromUtf8(name);
            if (within(file, project_->directory) && fs::is_regular_file(file))
            {
                openSource(file);
                opened = true;
            }
        }
        if (!opened && fs::is_regular_file(project_->directory / "src/main.cpp"))
            openSource(project_->directory / "src/main.cpp");
    }
    void tree()
    {
        if (!project_)
            return;
        if (projectWindow_)
            TObject::destroy(projectWindow_);
        auto r = deskTop->getExtent();
        r.a.x = std::max(0, r.b.x - 27);
        r.b.y = std::max(8, r.b.y - 6);
        r.b.y = std::max(6, r.b.y / 2);
        projectWindow_ = new PanelWindow(r, project_->name.c_str());
        std::vector<std::shared_ptr<tvision::TreeNode>> roots;
        auto forms = std::make_shared<tvision::TreeNode>("Формы");
        forms->expanded = true;
        for (auto &f : project_->forms)
        {
            auto n = std::make_shared<tvision::TreeNode>(f.name);
            n->data = "form:" + f.name;
            forms->children.push_back(n);
        }
        if (!project_->plainCpp)
            roots.push_back(forms);
        auto files = std::make_shared<tvision::TreeNode>("Исходники");
        files->expanded = true;
        for (auto &file : project_->sourceFiles())
        {
            auto n =
                std::make_shared<tvision::TreeNode>(utf8(fs::relative(file, project_->directory)));
            n->data = utf8(file);
            files->children.push_back(n);
        }
        roots.push_back(files);
        auto *filesView = new ProjectFiles(
            TRect(1, 1, projectWindow_->size.x - 1, projectWindow_->size.y - 1), roots);
        filesView->activate = [this](const std::string &path)
        {
            if (path.rfind("form:", 0) == 0)
                openForm(path.substr(5));
            else
                openSource(pathFromUtf8(path));
        };
        tree_ = filesView;
        tree_->growMode = gfGrowHiX | gfGrowHiY;
        projectWindow_->insert(tree_);
        deskTop->insert(projectWindow_);
    }
    void inspect(Designer *d)
    {
        inspected_ = d;
        if (!inspectorWindow_)
        {
            auto r = deskTop->getExtent();
            r.a.x = std::max(0, r.b.x - 27);
            r.b.y = std::max(8, r.b.y - 6);
            r.a.y = std::max(6, r.b.y / 2);
            inspectorWindow_ = new PanelWindow(r, "Свойства — Enter");
            auto model = std::make_shared<tvision::VecTableModel>();
            inspector_ = new tvision::TTable(
                TRect(1, 1, inspectorWindow_->size.x - 1, inspectorWindow_->size.y - 1), model,
                {{"Поле", 11}, {"Значение", 24}});
            inspector_->growMode = gfGrowHiX | gfGrowHiY;
            inspectorWindow_->insert(inspector_);
            deskTop->insert(inspectorWindow_);
        }
        Json j = d->document.selection.size() == 1
                     ? d->document.form.controls[*d->document.selection.begin()].toJson()
                     : d->document.form.toJson();
        std::vector<std::vector<std::string>> rows;
        for (auto &field : j.object)
            if (field.first != "format" && field.first != "version" && field.first != "controls")
                rows.push_back({field.first, field.second.kind == Json::Kind::Array
                                                 ? tvision::writeJson(field.second)
                                                 : field.second.display()});
        inspector_->setModel(std::make_shared<tvision::VecTableModel>(
            std::vector<std::string>{"Поле", "Значение"}, rows));
    }
    void openForm(const std::string &name)
    {
        if (!project_)
            return;
        auto it = forms_.find(name);
        if (it != forms_.end())
        {
            it->second->show();
            it->second->select();
            return;
        }
        int i = project_->formIndex(name);
        if (i < 0)
            return;
        auto *view = new Designer(project_->forms[size_t(i)], project_->app);
        view->changed = [this, view] { inspect(view); };
        forms_[name] = view;
        deskTop->insert(view);
        inspect(view);
        view->select();
    }
    SourceWindow *openSource(const fs::path &path)
    {
        auto absolute = path.empty() ? fs::path{} : fs::absolute(path).lexically_normal();
        for (auto *s : sources_)
            if (!absolute.empty() && s->path == absolute)
            {
                s->closed = false;
                s->show();
                s->select();
                return s;
            }
        auto *s = new SourceWindow(editorArea(), absolute);
        sources_.push_back(s);
        deskTop->insert(s);
        return s;
    }
    void properties()
    {
        auto *d = designer();
        if (!d && inspectorWindow_ && (inspectorWindow_->state & sfSelected))
            d = inspected_;
        if (!d)
            return;
        bool control = d->document.selection.size() == 1;
        size_t index = control ? *d->document.selection.begin() : 0;
        Json value =
            control ? d->document.form.controls[index].toJson() : d->document.form.toJson();
        auto text = writeYaml(value);
        while (editDocument(control ? "Свойства компонента (YAML)" : "Свойства формы (YAML)", text))
        {
            try
            {
                auto form = d->document.form;
                if (control)
                    form.controls[index] = Control::fromJson(parseDocument(text));
                else
                    form = Form::fromJson(parseDocument(text));
                if (form.name != d->document.form.name)
                    throw std::runtime_error(
                        "Имя открытой формы менять нельзя; создайте новую форму");
                auto errors = form.validate();
                if (!errors.empty())
                    throw std::runtime_error(joinLines(errors));
                auto *probe = makeForm(form, {}, project_->app);
                TObject::destroy(probe);
                d->document.checkpoint();
                d->document.form = std::move(form);
                auto r = d->getBounds();
                r.b.x = r.a.x + d->document.form.w;
                r.b.y = r.a.y + d->document.form.h;
                d->changeBounds(r);
                d->rebuild();
                return;
            }
            catch (const std::exception &e)
            {
                notice(e.what(), true);
            }
        }
    }
    void palette()
    {
        auto *d = designer();
        if (!d)
            return;
        Application a;
        auto area = deskTop->getExtent();
        auto *dialog = new TDialog(TRect(0, 0, 52, 20), "Компоненты TVisioAllIn");
        dialog->options |= ofCentered;
        std::vector<std::vector<std::string>> rows;
        for (auto &kind : kinds())
            rows.push_back({kind.id, kind.title});
        auto model = std::make_shared<tvision::VecTableModel>(
            std::vector<std::string>{"Класс", "Название"}, rows);
        auto *table =
            new tvision::TTable(TRect(2, 2, 50, 15), model, {{"Класс", 20}, {"Название", 26}});
        dialog->insert(table);
        dialog->insert(new TButton(TRect(5, 16, 22, 18), "Добавить", cmOK, bfDefault));
        dialog->insert(new TButton(TRect(28, 16, 45, 18), "Отмена", cmCancel, bfNormal));
        table->select();
        if (deskTop->execView(dialog) == cmOK && table->row() < kinds().size())
        {
            auto kind = kinds()[table->row()].kind;
            auto &k = info(kind);
            if (k.width >= d->document.form.w - 1 || k.height >= d->document.form.h - 1)
                notice("Увеличьте форму в свойствах", true);
            else
            {
                d->document.add(kind, 1, 1);
                d->rebuild();
            }
        }
        TObject::destroy(dialog);
    }
    void beginBuild(const std::string &action, bool run = false)
    {
        if (!project_)
            return;
        if (builder_.active())
        {
            notice("Сборка уже выполняется");
            return;
        }
        if (!saveAll())
            return;
        if (action == "build")
            generate(*project_);
        diagnostics_.clear();
        diagnostic_ = 0;
        outputTail_.clear();
        runAfter_ = run;
        builder_.start(*project_, action);
        building_ = true;
    }
    void runProgram()
    {
        auto exe = project_->executablePath();
        if (!fs::is_regular_file(exe))
            throw std::runtime_error("Не найден исполняемый файл: " + utf8(exe) +
                                     ". Укажите путь в настройках сборки");
        auto args = splitArguments(project_->build.runArguments);
        args.insert(args.begin(), utf8(exe));
        suspend();
        int result = -1;
        try
        {
            Process process;
            process.start(args, project_->directory, {}, {}, true);
            result = process.wait();
        }
        catch (...)
        {
            resume();
            tvision::refreshTheme(*this);
            throw;
        }
        std::cout << "\n[TV Studio] exit=" << result << ". Enter..." << std::flush;
        std::string line;
        std::getline(std::cin, line);
        resume();
        tvision::refreshTheme(*this);
        log("\nКод завершения: " + std::to_string(result) + "\n");
    }
    void nextError()
    {
        if (diagnostics_.empty())
        {
            notice("В выводе сборки нет распознанных ошибок");
            return;
        }
        auto &d = diagnostics_[diagnostic_++ % diagnostics_.size()];
        openSource(d.file)->editor->go(d.line, d.column);
        log(d.message + "\n");
    }
    void syncLsp()
    {
        if (!project_)
            return;
        auto now = std::chrono::steady_clock::now();
        if (now - lspSync_ < std::chrono::milliseconds(350))
            return;
        lspSync_ = now;
        std::set<fs::path> files;
        for (auto *s : sources_)
        {
            auto ext = utf8(s->path.extension());
            if (s->closed || s->path.empty() ||
                (ext != ".c" && ext != ".cpp" && ext != ".cc" && ext != ".cxx" &&
                 ext != ".h" && ext != ".hpp" && ext != ".hh" && ext != ".hxx"))
                continue;
            if (!lspAttempted_)
            {
                lspAttempted_ = true;
                try
                {
                    lsp_.start(*project_, str(project_->extra, "clangd", defaultClangdCommand()));
                }
                catch (const std::exception &error)
                {
                    log(std::string("clangd: не удалось запустить сервер: ") + error.what() +
                        "\nУкажите путь в меню «Проект → clangd: команда запуска».\n");
                    return;
                }
                log("clangd: запуск сервера\n");
            }
            if (lsp_.running())
            {
                lsp_.sync(s->path, editorText(*s->editor));
                files.insert(s->path);
            }
        }
        if (lsp_.running())
            for (auto &file : lspFiles_)
                if (!files.count(file))
                    lsp_.close(file);
        lspFiles_ = std::move(files);
    }
    void queryLsp(const std::string &method)
    {
        auto *s = source();
        if (!s || !project_ || s->path.empty())
            return;
        if (!lsp_.running())
            lsp_.start(*project_, str(project_->extra, "clangd", defaultClangdCommand()));
        lsp_.query(method, s->path, editorText(*s->editor), s->editor->curPtr);
    }
    void lspEvents()
    {
        for (auto &event : lsp_.poll())
        {
            if (event.error)
            {
                log("clangd: " + event.text + event.result.display() + "\n");
                continue;
            }
            if (event.tag == "ready" || event.tag == "log")
                log(event.text);
            if (event.tag == "diagnostics")
            {
                diagnostics_.erase(
                    std::remove_if(diagnostics_.begin(), diagnostics_.end(), [&](auto &d)
                                   { return d.source == "clangd" && d.file == event.file; }),
                    diagnostics_.end());
                for (auto &value : event.result.array)
                {
                    Diagnostic d;
                    d.file = event.file;
                    d.source = "clangd";
                    d.message = str(value, "message");
                    d.severity = int(integer(value, "severity", 1));
                    d.line = int(integer(get(get(value, "range"), "start"), "line")) + 1;
                    d.column = int(integer(get(get(value, "range"), "start"), "character")) + 1;
                    diagnostics_.push_back(d);
                    log(utf8(d.file) + ":" + std::to_string(d.line) + ": " + d.message + "\n");
                }
            }
            else if (event.tag == "completion")
            {
                SourceWindow *s = nullptr;
                for (auto *view : sources_)
                    if (view->path == event.file)
                        s = view;
                if (!s || editorText(*s->editor) != event.text)
                    continue;
                auto items = event.result.kind == Json::Kind::Array ? event.result
                                                                    : get(event.result, "items");
                if (items.array.empty())
                    continue;
                std::vector<std::vector<std::string>> rows;
                for (auto &item : items.array)
                    rows.push_back({str(item, "label"), str(item, "detail")});
                auto *dialog = new TDialog(TRect(0, 0, 74, 18), "clangd — дополнение");
                dialog->options |= ofCentered;
                auto model = std::make_shared<tvision::VecTableModel>(
                    std::vector<std::string>{"Символ", "Тип"}, rows);
                auto *table =
                    new tvision::TTable(TRect(2, 2, 72, 13), model, {{"Символ", 36}, {"Тип", 30}});
                dialog->insert(table);
                dialog->insert(new TButton(TRect(14, 14, 33, 16), "Вставить", cmOK, bfDefault));
                dialog->insert(new TButton(TRect(40, 14, 59, 16), "Отмена", cmCancel, bfNormal));
                table->select();
                if (deskTop->execView(dialog) == cmOK && table->row() < items.array.size())
                {
                    auto item = items.array[table->row()];
                    Json edits = get(item, "additionalTextEdits");
                    if (edits.kind == Json::Kind::Null)
                        edits = Json::list();
                    auto edit = get(item, "textEdit");
                    if (edit.kind == Json::Kind::Null)
                    {
                        size_t p = s->editor->curPtr;
                        while (p && (std::isalnum(static_cast<unsigned char>(event.text[p - 1])) ||
                                     event.text[p - 1] == '_'))
                            --p;
                        edit = Json::record(
                            {{"range",
                              Json::record({{"start", textPosition(event.text, p, lsp_.encoding())},
                                            {"end", textPosition(event.text, s->editor->curPtr,
                                                                 lsp_.encoding())}})},
                             {"newText",
                              Json::string(str(item, "insertText", str(item, "label")))}});
                    }
                    else if (get(edit, "range").kind == Json::Kind::Null)
                        put(edit, "range", get(edit, "replace"));
                    edits.array.push_back(edit);
                    auto text = applyTextEdits(event.text, edits, lsp_.encoding());
                    s->editor->snapshot();
                    replaceEditorText(*s->editor, text);
                    s->editor->modified = True;
                }
                TObject::destroy(dialog);
            }
            else if (event.tag == "definition" || event.tag == "references")
            {
                auto locations = event.result.kind == Json::Kind::Array
                                     ? event.result.array
                                     : std::vector<Json>{event.result};
                bool first = true;
                for (auto &location : locations)
                {
                    auto uri = str(location, "uri", str(location, "targetUri"));
                    if (uri.empty())
                        continue;
                    auto file = uriFile(uri);
                    auto range = get(location, "range");
                    if (range.kind == Json::Kind::Null)
                        range = get(location, "targetSelectionRange");
                    auto position = get(range, "start");
                    log(utf8(file) + ":" + std::to_string(integer(position, "line") + 1) + "\n");
                    if (first)
                    {
                        auto *s = openSource(file);
                        auto offset = textOffset(editorText(*s->editor), position, lsp_.encoding());
                        s->editor->setSelect(uint(offset), uint(offset), False);
                        s->editor->trackCursor(False);
                        first = false;
                    }
                }
                if (first)
                    log("clangd: символ не найден\n");
            }
        }
    }
    void command(ushort c)
    {
        switch (c)
        {
        case NewSource:
            openSource({});
            break;
        case CodeEditor:
            if (auto *s = source())
            {
                s->show();
                s->select();
            }
            else
            {
                SourceWindow *last = nullptr;
                for (auto *s : sources_)
                    if (!s->closed)
                        last = s;
                if (last)
                {
                    last->show();
                    last->select();
                }
                else if (project_ && fs::is_regular_file(project_->directory / "src/main.cpp"))
                    openSource(project_->directory / "src/main.cpp");
                else
                    openSource({});
            }
            break;
        case FormDesigner:
            if (project_ && !project_->forms.empty())
                openForm(project_->forms.front().name);
            break;
        case NewProject:
        {
            std::string name = "application", dir, lib = utf8(library_);
            if (!ask("Новый проект", "Имя (C++):", name) ||
                !ask("Новый проект", "Пустой каталог:", dir) || dir.empty() ||
                !ask("TVisioAllIn", "Каталог библиотеки:", lib))
                return;
            auto path = pathFromUtf8(dir);
            if (fs::exists(path) && !fs::is_empty(path))
                throw std::runtime_error("Нужен пустой каталог");
            auto p = Project::create(name, path, pathFromUtf8(lib));
            generate(p);
            load(p.file);
            break;
        }
        case OpenProject:
        {
            std::string path;
            if (ask("Открыть проект", "Файл .tvsp или каталог CMake:", path) && !path.empty())
                load(pathFromUtf8(path));
            break;
        }
        case Import:
        {
            std::string src, dest, name = "application";
            if (ask("Импорт", "Исходный каталог / main.cpp:", src) &&
                ask("Импорт", "Пустой каталог назначения:", dest) &&
                ask("Импорт", "Имя проекта:", name) && !src.empty() && !dest.empty())
            {
                auto p = importProject(pathFromUtf8(src), pathFromUtf8(dest), name, library_);
                load(p.file);
            }
            break;
        }
        case OpenFile:
        {
            std::string path = project_ ? utf8(project_->directory) + "/" : "";
            if (ask("Исходник", "Путь к файлу:", path) && !path.empty())
                openSource(pathFromUtf8(path));
            break;
        }
        case Save:
            if (auto *s = source())
            {
                if (s->save() && project_)
                {
                    tree();
                    s->select();
                }
            }
            else if (!saveAll())
                return;
            break;
        case SaveAll:
            saveAll();
            break;
        case NewForm:
        {
            if (!project_ || project_->plainCpp)
                return;
            std::string name = "dialog";
            if (ask("Новая форма", "Имя C++:", name))
            {
                Form f;
                f.name = name;
                f.title = name;
                if (!identifier(name) || project_->formIndex(name) >= 0)
                    throw std::runtime_error("Недопустимое или занятое имя формы");
                project_->forms.push_back(f);
                tree();
                openForm(name);
            }
            break;
        }
        case Properties:
            properties();
            break;
        case Palette:
            palette();
            break;
        case Preview:
            if (auto *d = designer())
            {
                Json data;
                previewForm(*this, d->document.form, data, project_->app);
                log("Данные формы:\n" + tvision::writeJson(data, true) + "\n");
            }
            break;
        case ProjectTree:
            if (projectWindow_)
            {
                projectWindow_->show();
                projectWindow_->select();
            }
            if (inspectorWindow_)
                inspectorWindow_->show();
            break;
        case Generate:
            if (project_)
            {
                if (!saveAll())
                    return;
                auto files = generate(*project_);
                log("Генерация: " + std::to_string(files.size()) + " файлов\n");
                tree();
            }
            break;
        case AppModel:
            if (project_ && !project_->plainCpp)
            {
                sync();
                auto text = writeYaml(project_->app.toJson());
                while (editDocument("Приложение: меню, команды, статус, стартовая форма", text))
                {
                    auto app = Application::fromJson(parseDocument(text));
                    auto old = project_->app;
                    project_->app = app;
                    auto errors = project_->validate();
                    if (!errors.empty())
                    {
                        project_->app = old;
                        notice(joinLines(errors), true);
                        continue;
                    }
                    project_->save();
                    break;
                }
            }
            break;
        case BuildOptionsCommand:
            if (project_)
            {
                auto text = writeYaml(get(project_->toJson(), "build"));
                if (editDocument(
                        "CMake: generator, compiler, profile, target, extra, run_args, executable",
                        text))
                {
                    auto j = project_->toJson();
                    put(j, "build", parseDocument(text));
                    auto backup = readFile(project_->file);
                    writeFile(project_->file, writeYaml(j));
                    try
                    {
                        auto p = Project::load(project_->file);
                        project_->build = p.build;
                    }
                    catch (...)
                    {
                        writeFile(project_->file, backup);
                        throw;
                    }
                }
            }
            break;
        case Build:
            beginBuild("build");
            break;
        case Run:
            beginBuild("build", true);
            break;
        case Test:
            beginBuild("test");
            break;
        case Clean:
            beginBuild("clean");
            break;
        case Stop:
            builder_.stop();
            runAfter_ = false;
            break;
        case NextError:
            nextError();
            break;
        case Find:
        case Replace:
            if (auto *s = source())
            {
                std::string find;
                if (ask("Поиск", "Текст:", find) && !find.empty())
                {
                    if (c == Find)
                    {
                        if (!s->editor->search(find.c_str(), 0))
                        {
                            s->editor->setSelect(0, 0, False);
                            s->editor->search(find.c_str(), 0);
                        }
                    }
                    else
                    {
                        std::string replacement;
                        if (ask("Заменить всё", "Замена:", replacement))
                        {
                            auto text = editorText(*s->editor);
                            size_t pos = 0;
                            while ((pos = text.find(find, pos)) != std::string::npos)
                            {
                                text.replace(pos, find.size(), replacement);
                                pos += replacement.size();
                            }
                            s->editor->snapshot();
                            replaceEditorText(*s->editor, text);
                        }
                    }
                }
            }
            break;
        case Goto:
            if (auto *s = source())
            {
                std::string line = "1";
                if (ask("Переход", "Номер строки:", line))
                    s->editor->go(std::stoi(line), 1);
            }
            break;
        case Complete:
            if (auto *s = source())
                if (!project_ || s->path.empty())
                {
                    s->editor->complete();
                    break;
                }
            try
            {
                queryLsp("completion");
            }
            catch (const std::exception &e)
            {
                log(std::string(e.what()) + "; дополнение словами из буфера\n");
                if (auto *s = source())
                    s->editor->complete();
            }
            break;
        case Definition:
            queryLsp("definition");
            break;
        case References:
            queryLsp("references");
            break;
        case Clangd:
            if (project_)
            {
                std::string value = str(project_->extra, "clangd", defaultClangdCommand());
                if (ask("clangd (необязательный LSP)", "Команда и аргументы:", value))
                {
                    lsp_.stop();
                    lspAttempted_ = false;
                    lspFiles_.clear();
                    put(project_->extra, "clangd", Json::string(value));
                    project_->save();
                }
            }
            break;
        case cmUndo:
        case Redo:
        case cmCut:
        case cmCopy:
        case cmPaste:
            if (auto *d = designer())
            {
                if (c == cmUndo)
                    d->document.undo();
                if (c == Redo)
                    d->document.redo();
                if (c == cmCut)
                    d->document.cut();
                if (c == cmCopy)
                    d->document.copy();
                if (c == cmPaste)
                    d->document.paste();
                d->rebuild();
            }
            else if (auto *s = source())
            {
                TEvent e;
                e.what = evCommand;
                e.message.command = c;
                s->editor->handleEvent(e);
            }
            break;
        case AlignLeft:
        case AlignTop:
        case AlignWidth:
            if (auto *d = designer())
            {
                d->document.align(c == AlignLeft ? "left" : c == AlignTop ? "top" : "width");
                d->rebuild();
            }
            break;
        case TabForward:
        case TabBack:
            if (auto *d = designer())
            {
                d->document.tabOrder(c == TabForward ? 1 : -1);
                d->rebuild();
            }
            break;
        case ThemeCommand:
            setTheme(theme() == tvision::Theme::Gruvbox    ? tvision::Theme::Twilight
                     : theme() == tvision::Theme::Twilight ? tvision::Theme::Classic
                                                           : tvision::Theme::Gruvbox);
            break;
        case About:
            notice("TV Studio C++\nTVisioAllIn / C++17 / CMake\nПо мотивам cat386/tvstudio\n14 "
                   "компонентов, YAML, генерация C++, редактор, сборка");
            break;
        case Help:
            notice("F3: дерево проекта; Enter открывает файл или форму.\nДизайнер: щелчок "
                   "выбирает, Ctrl — несколько.\nМышь перемещает, Shift+мышь меняет "
                   "размер.\nСтрелки перемещают, Shift+стрелки меняют размер.\nInsert — компонент, "
                   "Enter — свойства YAML.\nCtrl+C/X/V/Z — "
                   "копировать/вырезать/вставить/отменить.\nF9 — рабочий предпросмотр с чтением "
                   "данных.\nF7 — CMake; Ctrl+F9 — сборка и запуск; F8 — ошибка.\nРедактор: "
                   "Ctrl+F/R/G, Ctrl+Space, F2.\nПриложение/сборка настраиваются в меню «Проект».");
            break;
        default:
            break;
        }
    }

  public:
    Studio(const fs::path &path, const fs::path &library)
        : TProgInit(&Studio::status, &Studio::menu, &TApplication::initDeskTop), library_(library)
    {
        TEditor::clipboard = new TFileEditor(TRect(0, 0, 1, 1), nullptr, nullptr, nullptr, "");
        if (!path.empty())
            load(path);
        else
        {
            log("TV Studio C++ — создайте или откройте проект в меню «Файл». F1: помощь.\n");
            openSource({});
        }
    }
    void handleEvent(TEvent &e) override
    {
        try
        {
            if (e.what == evKeyDown &&
                TKey(e.keyDown.keyCode, e.keyDown.controlKeyState) == TKey('X', kbAltShift))
            {
                e.what = evCommand;
                e.message.command = cmQuit;
                e.message.infoPtr = nullptr;
            }
            if (e.what == evCommand && e.message.command == cmQuit)
            {
                if (closeProject())
                    TThemedApplication::handleEvent(e);
                else
                    clearEvent(e);
                return;
            }
            if (e.what == evKeyDown)
            {
                auto key = e.keyDown.keyCode;
                ushort c = 0;
                if (key == kbF1)
                    c = Help;
                else if (key == kbF2)
                    c = Save;
                else if (key == kbF3)
                    c = ProjectTree;
                else if (TKey(key, e.keyDown.controlKeyState) == TKey(kbF4, kbShift))
                    c = FormDesigner;
                else if (key == kbF4)
                    c = CodeEditor;
                else if (key == kbCtrlN)
                    c = NewSource;
                else if (key == kbF7)
                    c = Build;
                else if (key == kbF8)
                    c = NextError;
                else if (key == kbF9)
                    c = Preview;
                else if (key == kbCtrlF9)
                    c = Run;
                else if (key == kbCtrlF2)
                    c = Stop;
                else if (key == kbCtrlS)
                    c = SaveAll;
                else if (key == kbCtrlO)
                    c = OpenProject;
                else if (key == kbCtrlF)
                    c = Find;
                else if (key == kbCtrlR)
                    c = Replace;
                else if (key == kbCtrlG)
                    c = Goto;
                else if (TKey(key, e.keyDown.controlKeyState) == TKey(' ', kbCtrlShift))
                    c = Complete;
                else if (key == kbF12)
                    c = Definition;
                else if (designer() && key == kbIns)
                    c = Palette;
                else if (designer() && key == kbEnter)
                    c = Properties;
                else if (inspectorWindow_ && (inspectorWindow_->state & sfSelected) &&
                         key == kbEnter)
                    c = Properties;
                else if (tree_ && (projectWindow_->state & sfSelected) && key == kbEnter)
                {
                    auto node = tree_->current();
                    if (node && !node->data.empty())
                    {
                        auto path = node->data;
                        if (path.rfind("form:", 0) == 0)
                            openForm(path.substr(5));
                        else
                            openSource(pathFromUtf8(path));
                    }
                    clearEvent(e);
                    return;
                }
                if (c)
                {
                    command(c);
                    clearEvent(e);
                    return;
                }
            }
            if (e.what == evCommand &&
                ((e.message.command >= NewProject && e.message.command <= Help) ||
                 e.message.command == cmUndo || e.message.command == cmCopy ||
                 e.message.command == cmCut || e.message.command == cmPaste))
            {
                command(e.message.command);
                clearEvent(e);
                return;
            }
            TThemedApplication::handleEvent(e);
        }
        catch (const std::exception &error)
        {
            clearEvent(e);
            notice(error.what(), true);
        }
    }
    void idle() override
    {
        TThemedApplication::idle();
        if (designer())
            for (ushort c : {cmUndo, cmCopy, cmCut, cmPaste})
                enableCommand(c);
        try
        {
            syncLsp();
            lspEvents();
            builder_.poll();
            for (auto &chunk : builder_.takeOutput())
            {
                log(chunk);
                outputTail_ += chunk;
                for (size_t newline; (newline = outputTail_.find('\n')) != std::string::npos;)
                {
                    auto line = outputTail_.substr(0, newline);
                    outputTail_.erase(0, newline + 1);
                    Diagnostic d;
                    if (project_ && parseDiagnostic(line, project_->directory, d))
                        diagnostics_.push_back(d);
                }
            }
            if (building_ && !builder_.active())
            {
                building_ = false;
                log("\nЗавершено: " + std::to_string(builder_.result()) + "\n");
                bool run = runAfter_ && builder_.result() == 0;
                runAfter_ = false;
                if (run)
                    runProgram();
            }
        }
        catch (const std::exception &e)
        {
            runAfter_ = false;
            building_ = false;
            log(std::string(e.what()) + "\n");
        }
    }
    void shutDown() override
    {
        builder_.stop();
        lsp_.stop();
        if (TEditor::clipboard)
        {
            TObject::destroy(TEditor::clipboard);
            TEditor::clipboard = nullptr;
        }
        TThemedApplication::shutDown();
    }
};
} // namespace
} // namespace tvstudio

int main(int argc, char **argv)
{
    using namespace tvstudio;
    try
    {
        auto args = commandLineArguments(argc, argv);
        auto library = pathFromUtf8(TVSTUDIO_SOURCE_ROOT);
        auto need = [&](size_t count)
        {
            if (args.size() < count)
                throw std::runtime_error("Недостаточно аргументов; используйте --help");
        };
        if (args.size() > 1 && args[1] == "--help")
        {
            std::cout << "TV Studio C++\n tvstudio [project.tvsp | CMake-directory]\n --new NAME "
                         "DIRECTORY [TVISION_DIRECTORY]\n --generate PROJECT\n --check PROJECT\n "
                         "--import SOURCE DESTINATION NAME [TVISION_DIRECTORY]\n --smoke\n";
            return 0;
        }
        if (args.size() > 1 && args[1] == "--new")
        {
            need(4);
            auto p = Project::create(args[2], pathFromUtf8(args[3]),
                                     args.size() > 4 ? pathFromUtf8(args[4]) : library);
            if (fs::exists(p.directory) && !fs::is_empty(p.directory))
                throw std::runtime_error("Нужен пустой каталог");
            generate(p);
            std::cout << utf8(p.file) << '\n';
            return 0;
        }
        if (args.size() > 1 && args[1] == "--generate")
        {
            need(3);
            auto p = Project::load(pathFromUtf8(args[2]));
            auto files = generate(p);
            std::cout << files.size() << " generated files\n";
            return 0;
        }
        if (args.size() > 1 && args[1] == "--check")
        {
            need(3);
            auto p = Project::load(pathFromUtf8(args[2]));
            auto errors = p.validate();
            for (auto &s : errors)
                std::cerr << s << '\n';
            return errors.empty() ? 0 : 1;
        }
        if (args.size() > 1 && args[1] == "--import")
        {
            need(5);
            auto p = importProject(pathFromUtf8(args[2]), pathFromUtf8(args[3]), args[4],
                                   args.size() > 5 ? pathFromUtf8(args[5]) : library);
            std::cout << utf8(p.file) << '\n';
            return 0;
        }
        if (args.size() > 1 && args[1] == "--smoke")
        {
            auto *menu = makeMenuBar(TRect(0, 0, 100, 1), studioMenus());
            TObject::destroy(menu);
            Form f;
            f.w = 90;
            f.h = 45;
            for (auto &kind : kinds())
                f.controls.push_back(Control::make(kind.kind, kind.prefix, 2, 2));
            auto *v = makeForm(f);
            auto data = readForm(*v);
            TObject::destroy(v);
            auto *designer = new Designer(Form{}, {});
            for (auto &kind : kinds())
            {
                designer->document.add(kind.kind);
                designer->rebuild();
                designer->document.undo();
                designer->rebuild();
            }
            TObject::destroy(designer);
            auto *editor = new SourceEditor(TRect(0, 0, 40, 10), nullptr, nullptr, nullptr);
            replaceEditorText(*editor, "int main() { return 0; }\n");
            editor->snapshot();
            replaceEditorText(*editor, "int main() { return 1; }\n");
            editor->restore(false);
            if (editorText(*editor).find("return 0") == std::string::npos)
                throw std::runtime_error("Editor undo");
            TObject::destroy(editor);
            auto file =
                fs::temp_directory_path() /
                pathFromUtf8(
                    "tvstudio-editor-" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                    ".cpp");
            try
            {
                writeFile(file, "int main() {}\n", false);
                auto *window = new SourceWindow(TRect(0, 0, 60, 15), file);
                if (window->editor->curPtr != 0 || window->editor->curPos.y != 0)
                    throw std::runtime_error("Editor initial cursor");
                window->editor->insertText("// check\n", 9, False);
                if (!window->save() || readFile(file).find("// check") != 0)
                    throw std::runtime_error("Editor save");
                TObject::destroy(window);
                auto *empty = new SourceWindow(TRect(0, 0, 60, 15), {});
                if (empty->editor->bufLen || !empty->path.empty())
                    throw std::runtime_error("Untitled editor");
                TObject::destroy(empty);
            }
            catch (...)
            {
                fs::remove(file);
                fs::remove(pathFromUtf8(utf8(file) + ".bak"));
                throw;
            }
            fs::remove(file);
            fs::remove(pathFromUtf8(utf8(file) + ".bak"));
            std::cout << "TV Studio: 14 widgets, menus, editor, data OK\n";
            return 0;
        }
        if (args.size() > 1 && args[1].rfind("--", 0) == 0)
            throw std::runtime_error("Неизвестная команда: " + args[1]);
        Studio app(args.size() > 1 ? pathFromUtf8(args[1]) : fs::path{}, library);
        app.run();
        app.shutDown();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "TV Studio: " << e.what() << '\n';
        return 1;
    }
}
