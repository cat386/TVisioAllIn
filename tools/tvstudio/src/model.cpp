#include <algorithm>
#include <climits>
#include <functional>
#include <stdexcept>
#include <tvstudio/model.h>

namespace tvstudio
{
namespace
{
int readInt(const Json &j, const std::string &key, int fallback, int low = -32760, int high = 32760)
{
    auto n = integer(j, key, fallback);
    if (n < low || n > high)
        throw std::runtime_error("Поле вне диапазона: " + key);
    return int(n);
}
Json strings(const std::vector<std::string> &v)
{
    Json out = Json::list();
    for (auto &s : v)
        out.array.push_back(Json::string(s));
    return out;
}
std::vector<std::string> stringArray(const Json &j)
{
    if (j.kind == Json::Kind::Null)
        return {};
    if (j.kind != Json::Kind::Array)
        throw std::runtime_error("Ожидается список строк");
    std::vector<std::string> out;
    for (const auto &v : j.array)
    {
        if (v.kind != Json::Kind::String)
            throw std::runtime_error("Элемент списка должен быть строкой");
        out.push_back(v.text);
    }
    return out;
}
void object(const Json &j)
{
    if (j.kind != Json::Kind::Object)
        throw std::runtime_error("Ожидается объект");
}
void format(const Json &j, const std::string &expected)
{
    object(j);
    auto f = str(j, "format");
    if (!f.empty() && f != expected)
        throw std::runtime_error("Неверный формат: " + f);
    if (integer(j, "version", 1) != 1)
        throw std::runtime_error("Неподдерживаемая версия файла");
}
Json menuJson(const Menu &m)
{
    if (m.separator)
        return Json::record({{"separator", Json::flag(true)}});
    Json j = Json::record({{"title", Json::string(m.title)},
                           {"key", Json::string(m.key)},
                           {"help", Json::number(std::to_string(m.help))}});
    if (!m.items.empty())
    {
        Json a = Json::list();
        for (auto &c : m.items)
            a.array.push_back(menuJson(c));
        put(j, "items", std::move(a));
    }
    else
        put(j, "command", Json::string(m.command));
    return j;
}
Menu menuFrom(const Json &j, unsigned depth = 0)
{
    object(j);
    if (depth > 32)
        throw std::runtime_error("Слишком глубокое меню");
    Menu m;
    m.title = str(j, "title");
    m.command = str(j, "command");
    m.key = str(j, "key");
    m.help = readInt(j, "help", 0, 0, 65535);
    m.separator = boolean(j, "separator", false);
    for (auto &v : get(j, "items").array)
        m.items.push_back(menuFrom(v, depth + 1));
    return m;
}
bool projectName(const std::string &s)
{
    if (s.empty() || s.size() > 100)
        return false;
    static const std::set<std::string> reserved = {
        "all",          "clean",    "help",           "install",
        "test",         "package",  "edit_cache",     "rebuild_cache",
        "tvision",      "tvstudio", "tvstudio-model", "tvstudio-runtime",
        "tvstudio-core"};
    if (reserved.count(s))
        return false;
    for (unsigned char c : s)
        if (c >= 128 || !(std::isalnum(c) || c == '_' || c == '-'))
            return false;
    return std::isalnum(static_cast<unsigned char>(s[0])) != 0;
}
} // namespace
const std::vector<KindInfo> &kinds()
{
    static const std::vector<KindInfo> all = {
        {Kind::Text, "static_text", "Текст", "text", 20, 1},
        {Kind::Label, "label", "Метка", "label", 12, 1},
        {Kind::Button, "button", "Кнопка", "button", 12, 2},
        {Kind::Input, "input_line", "Строка ввода", "input", 24, 1},
        {Kind::Checks, "check_boxes", "Флажки", "checks", 20, 2},
        {Kind::Radios, "radio_buttons", "Переключатели", "radios", 20, 2},
        {Kind::List, "list_box", "Список", "list", 24, 5},
        {Kind::Memo, "memo", "Многострочное поле", "memo", 30, 4},
        {Kind::Combo, "combo_box", "Поле со списком", "combo", 24, 1},
        {Kind::Scroll, "scroll_bar", "Полоса прокрутки", "scroll", 1, 6},
        {Kind::Progress, "progress_bar", "Полоса выполнения", "progress", 30, 1},
        {Kind::Table, "table", "Таблица", "table", 40, 6},
        {Kind::Tree, "tree_view", "Дерево", "tree", 30, 8},
        {Kind::Line, "line", "Линия", "line", 20, 1}};
    return all;
}
const KindInfo &info(Kind kind)
{
    for (auto &v : kinds())
        if (v.kind == kind)
            return v;
    throw std::runtime_error("Неизвестный вид элемента");
}
Kind parseKind(const std::string &id)
{
    for (auto &v : kinds())
        if (id == v.id)
            return v.kind;
    throw std::runtime_error("Неизвестный вид элемента: " + id);
}
bool Rect::contains(int px, int py) const
{
    return px >= x && py >= y && int64_t(px) < int64_t(x) + w && int64_t(py) < int64_t(y) + h;
}
bool Rect::intersects(const Rect &o) const
{
    return int64_t(x) < int64_t(o.x) + o.w && int64_t(o.x) < int64_t(x) + w &&
           int64_t(y) < int64_t(o.y) + o.h && int64_t(o.y) < int64_t(y) + h;
}
Control Control::make(Kind k, std::string n, int px, int py)
{
    Control c;
    c.kind = k;
    c.name = std::move(n);
    c.x = px;
    c.y = py;
    c.w = info(k).width;
    c.h = info(k).height;
    switch (k)
    {
    case Kind::Text:
        c.text = "Текст";
        break;
    case Kind::Label:
        c.text = "~М~етка";
        break;
    case Kind::Button:
        c.text = "O~K~";
        c.command = "cm::OK";
        break;
    case Kind::Checks:
    case Kind::Radios:
        c.items = {"Первый", "Второй"};
        break;
    case Kind::List:
        c.items = {"Элемент 1", "Элемент 2", "Элемент 3"};
        c.scrollbar = true;
        break;
    case Kind::Memo:
        c.scrollbar = true;
        break;
    case Kind::Combo:
        c.items = {"Один", "Два", "Три"};
        c.maxLength = 64;
        break;
    case Kind::Progress:
        c.value = 40;
        break;
    case Kind::Table:
        c.items = {"Имя:16", "Телефон:14"};
        c.text = "Аня|123-45-67\nБорис|234-56-78";
        c.editable = true;
        break;
    case Kind::Tree:
        c.items = {"Документы", "  Письма", "  Отчёты", "Музыка"};
        break;
    default:
        break;
    }
    return c;
}
Json Control::toJson() const
{
    Json j = extra;
    put(j, "kind", Json::string(info(kind).id));
    put(j, "name", Json::string(name));
    for (auto p : {std::pair<const char *, int>{"x", x},
                   {"y", y},
                   {"w", w},
                   {"h", h},
                   {"max_len", maxLength},
                   {"history", history},
                   {"columns", columns},
                   {"help_ctx", help}})
        put(j, p.first, Json::number(std::to_string(p.second)));
    for (auto p : {std::pair<const char *, std::string>{"text", text},
                   {"command", command},
                   {"link", link},
                   {"validator", validator}})
        put(j, p.first, Json::string(p.second));
    for (auto p : {std::pair<const char *, bool>{"default", defaultButton},
                   {"scrollbar", scrollbar},
                   {"header", header},
                   {"grid", grid},
                   {"editable", editable},
                   {"row_numbers", rowNumbers},
                   {"checks", checks},
                   {"lines", lines},
                   {"expanded", expanded},
                   {"vertical", vertical},
                   {"double", doubleLine},
                   {"to_frame", toFrame},
                   {"disabled", disabled}})
        put(j, p.first, Json::flag(p.second));
    put(j, "items", strings(items));
    put(j, "value", Json::number(std::to_string(value)));
    put(j, "max", Json::number(std::to_string(maximum)));
    return j;
}
Control Control::fromJson(const Json &j)
{
    object(j);
    auto c = make(parseKind(str(j, "kind")), str(j, "name"));
    c.extra = j;
    c.x = readInt(j, "x", 2);
    c.y = readInt(j, "y", 2);
    c.w = readInt(j, "w", c.w, 0);
    c.h = readInt(j, "h", c.h, 0);
    c.maxLength = readInt(j, "max_len", 64, 1, 65534);
    c.history = readInt(j, "history", 0, 0, 65535);
    c.columns = readInt(j, "columns", 1, 1, 128);
    c.help = readInt(j, "help_ctx", 0, 0, 65535);
    c.text = str(j, "text", c.text);
    c.command = str(j, "command", c.command);
    c.link = str(j, "link");
    c.validator = str(j, "validator");
    if (j.find("items"))
        c.items = stringArray(get(j, "items"));
    c.value = integer(j, "value", c.value);
    c.maximum = integer(j, "max", 100);
    c.defaultButton = boolean(j, "default", false);
    c.scrollbar = boolean(j, "scrollbar", c.scrollbar);
    c.header = boolean(j, "header", true);
    c.grid = boolean(j, "grid", true);
    c.editable = boolean(j, "editable", false);
    c.rowNumbers = boolean(j, "row_numbers", false);
    c.checks = boolean(j, "checks", false);
    c.lines = boolean(j, "lines", true);
    c.expanded = boolean(j, "expanded", true);
    c.vertical = boolean(j, "vertical", false);
    c.doubleLine = boolean(j, "double", false);
    c.toFrame = boolean(j, "to_frame", false);
    c.disabled = boolean(j, "disabled", false);
    return c;
}
bool Control::hasData() const
{
    return kind == Kind::Input || kind == Kind::Checks || kind == Kind::Radios ||
           kind == Kind::List || kind == Kind::Memo || kind == Kind::Combo ||
           kind == Kind::Progress || kind == Kind::Table;
}
std::string Form::uniqueName(const std::string &p) const
{
    for (unsigned n = 1;; ++n)
    {
        auto name = p + std::to_string(n);
        if (find(name) < 0)
            return name;
    }
}
int Form::find(const std::string &n) const
{
    for (size_t i = 0; i < controls.size(); ++i)
        if (controls[i].name == n)
            return int(i);
    return -1;
}
int Form::hit(int x, int y) const
{
    for (size_t i = controls.size(); i > 0; --i)
        if (controls[i - 1].contains(x, y))
            return int(i - 1);
    return -1;
}
Json Form::toJson() const
{
    Json j = extra;
    put(j, "format", Json::string("tvstudio-form"));
    put(j, "version", Json::number("1"));
    put(j, "name", Json::string(name));
    put(j, "title", Json::string(title));
    put(j, "palette", Json::string(palette));
    put(j, "class", Json::string(window ? "Window" : "Dialog"));
    for (auto p : {std::pair<const char *, int>{"x", x},
                   {"y", y},
                   {"w", w},
                   {"h", h},
                   {"number", number},
                   {"help_ctx", help}})
        put(j, p.first, Json::number(std::to_string(p.second)));
    put(j, "centered", Json::flag(centered));
    put(j, "resizable", Json::flag(resizable));
    Json a = Json::list();
    for (auto &c : controls)
        a.array.push_back(c.toJson());
    put(j, "controls", std::move(a));
    return j;
}
Form Form::fromJson(const Json &j)
{
    format(j, "tvstudio-form");
    Form f;
    f.extra = j;
    f.name = str(j, "name");
    f.title = str(j, "title", f.name);
    f.palette = str(j, "palette", "gray");
    auto className = str(j, "class", "Dialog");
    if (className != "Window" && className != "Dialog")
        throw std::runtime_error("Класс формы: Dialog или Window");
    f.window = className == "Window";
    f.x = readInt(j, "x", 5);
    f.y = readInt(j, "y", 2);
    f.w = readInt(j, "w", 50, 4, 2000);
    f.h = readInt(j, "h", 15, 3, 1000);
    f.number = readInt(j, "number", 0, 0, 9);
    f.help = readInt(j, "help_ctx", 0, 0, 65535);
    f.centered = boolean(j, "centered", true);
    f.resizable = boolean(j, "resizable", true);
    if (get(j, "controls").kind != Json::Kind::Null && get(j, "controls").kind != Json::Kind::Array)
        throw std::runtime_error("controls должен быть списком");
    if (get(j, "controls").array.size() > 4096)
        throw std::runtime_error("В форме слишком много элементов");
    for (auto &c : get(j, "controls").array)
        f.controls.push_back(Control::fromJson(c));
    return f;
}
Form Form::load(const fs::path &p)
{
    try
    {
        return fromJson(parseDocument(readFile(p)));
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(utf8(p) + ": " + e.what());
    }
}
void Form::save(const fs::path &p) const
{
    writeFile(p, writeYaml(toJson()));
}
std::vector<std::string> Form::validate() const
{
    std::vector<std::string> out;
    auto issue = [&](const std::string &s) { out.push_back(name + ": " + s); };
    if (!identifier(name))
        issue("неверное имя формы C++");
    if (name == "forms" || name == "std" || name == "tvstudio" || name == "tvision")
        issue("имя формы занято генератором");
    if (w < 4 || h < 3 || w > 2000 || h > 1000)
        issue("неверный размер формы");
    if (palette != "gray" && palette != "blue" && palette != "cyan")
        issue("неизвестная палитра");
    std::set<std::string> names;
    unsigned defaults = 0;
    for (auto &c : controls)
    {
        if (!identifier(c.name))
            issue("неверное имя элемента: " + c.name);
        if (c.hasData() && c.name == "Data")
            issue("имя Data занято структурой данных");
        if (!names.insert(c.name).second)
            issue("повторное имя: " + c.name);
        if (c.w < 1 || c.h < 1)
            issue("нулевой размер: " + c.name);
        int edge = c.kind == Kind::Line && c.toFrame ? 0 : 1;
        if (c.x < edge || c.y < edge || int64_t(c.x) + c.w > w - edge ||
            int64_t(c.y) + c.h > h - edge)
            issue("выход за рамку: " + c.name);
        if (c.kind == Kind::Label && !c.link.empty() && find(c.link) < 0)
            issue("метка связана с отсутствующим элементом: " + c.name);
        if (c.kind == Kind::Checks && c.items.size() > 16)
            issue("не более 16 флажков: " + c.name);
        if (c.items.size() > 16000)
            issue("слишком длинный список: " + c.name);
        if ((c.kind == Kind::Input || c.kind == Kind::Combo) && c.history && c.w < 4)
            issue("истории ввода нужно не менее четырёх колонок: " + c.name);
        if (c.kind == Kind::List && c.scrollbar && c.w < 2)
            issue("списку с прокруткой нужно не менее двух колонок: " + c.name);
        if (c.kind == Kind::Button && c.defaultButton)
            ++defaults;
        if (c.text.find('\0') != std::string::npos)
            issue("нулевой символ в тексте: " + c.name);
        if (c.kind == Kind::Combo)
            for (auto &s : c.items)
                if (s.size() > (size_t)c.maxLength)
                    issue("пункт списка длиннее max_len: " + c.name);
    }
    if (defaults > 1)
        issue("несколько кнопок по умолчанию");
    return out;
}
Application Application::starter(const std::string &title)
{
    Application a;
    a.title = title;
    a.commands = {{"ABOUT", "form:about", "Открыть сведения о программе", 100}};
    Menu file;
    file.title = "~Ф~айл";
    file.items = {{"~В~ыход", "cm::QUIT", "Alt+X"}};
    Menu help;
    help.title = "~С~правка";
    help.items = {{"~О~ программе", "ABOUT", "F1"}};
    a.menu = {file, help};
    a.status = {{"~F1~ Справка", "F1", "ABOUT"}, {"~Alt-X~ Выход", "Alt+X", "cm::QUIT"}};
    return a;
}
Json Application::toJson() const
{
    Json j = Json::record({{"title", Json::string(title)},
                           {"startup_form", Json::string(startup)},
                           {"russian", Json::flag(true)}});
    Json menus = Json::list(), statuses = Json::list(), cmds = Json::list();
    for (auto &m : menu)
        menus.array.push_back(menuJson(m));
    for (auto &s : status)
        statuses.array.push_back(Json::record({{"text", Json::string(s.text)},
                                               {"key", Json::string(s.key)},
                                               {"command", Json::string(s.command)}}));
    for (auto &c : commands)
        cmds.array.push_back(Json::record({{"name", Json::string(c.name)},
                                           {"value", Json::number(std::to_string(c.value))},
                                           {"action", Json::string(c.action)},
                                           {"comment", Json::string(c.comment)}}));
    put(j, "menu", std::move(menus));
    put(j, "status", std::move(statuses));
    put(j, "commands", std::move(cmds));
    return j;
}
Application Application::fromJson(const Json &j)
{
    Application a;
    if (j.kind == Json::Kind::Null)
        return a;
    object(j);
    a.title = str(j, "title");
    a.startup = str(j, "startup_form");
    for (auto &v : get(j, "menu").array)
        a.menu.push_back(menuFrom(v));
    for (auto &v : get(j, "status").array)
        a.status.push_back({str(v, "text"), str(v, "key"), str(v, "command")});
    for (auto &v : get(j, "commands").array)
        a.commands.push_back({str(v, "name"), str(v, "action"), str(v, "comment"),
                              unsigned(readInt(v, "value", 100, 1, 65535))});
    return a;
}
Project Project::create(std::string name, const fs::path &dir, const fs::path &lib, bool plain)
{
    if (!projectName(name))
        throw std::runtime_error("Имя проекта: только латинские буквы, цифры, '_' и '-'");
    Project p;
    p.name = std::move(name);
    p.directory = fs::absolute(dir).lexically_normal();
    p.file = p.directory / pathFromUtf8(p.name + ".tvsp");
    p.library = fs::absolute(lib).lexically_normal();
    p.plainCpp = plain;
    p.app = Application::starter(p.name);
    p.openFiles = {"src/main.cpp"};
    if (!plain)
    {
        Form f;
        auto label = Control::make(Kind::Text, "title", 3, 2);
        label.text = "Создано в TV Studio C++";
        label.w = 42;
        auto button = Control::make(Kind::Button, "ok", 18, 9);
        button.defaultButton = true;
        f.controls = {label, button};
        p.forms.push_back(f);
    }
    return p;
}
Project Project::load(const fs::path &input)
{
    fs::path file = input;
    if (fs::is_directory(file))
    {
        std::vector<fs::path> projects;
        for (auto &e : fs::directory_iterator(file))
            if (e.is_regular_file() && e.path().extension() == ".tvsp")
                projects.push_back(e.path());
        std::sort(projects.begin(), projects.end());
        if (!projects.empty())
            file = projects.front();
        else if (fs::exists(file / "CMakeLists.txt"))
        {
            auto p = create(cppName(utf8(file.filename())), file, file, true);
            p.openFiles.clear();
            for (auto candidate : {"src/main.cpp", "main.cpp", "src/main.cc", "main.cc"})
                if (fs::exists(file / candidate))
                {
                    p.openFiles.push_back(candidate);
                    break;
                }
            return p;
        }
        else
            throw std::runtime_error("В каталоге нет проекта .tvsp или CMakeLists.txt");
    }
    auto j = parseDocument(readFile(file));
    format(j, "tvstudio-project");
    Project p;
    p.extra = j;
    p.file = fs::absolute(file).lexically_normal();
    p.directory = p.file.parent_path();
    p.name = str(j, "name", utf8(file.stem()));
    auto kind = str(j, "kind", "tvision-cpp");
    p.plainCpp = kind == "cpp";
    if (kind == "rust")
        throw std::runtime_error("Проект Rust нужно импортировать в новый проект C++");
    p.app = Application::fromJson(get(j, "app"));
    p.library = pathFromUtf8(str(j, "tvision_path"));
    if (!p.library.empty() && p.library.is_relative())
        p.library = p.directory / p.library;
    for (auto &name : stringArray(get(j, "forms")))
    {
        if (!identifier(name))
            throw std::runtime_error("Недопустимое имя формы: " + name);
        auto path = p.directory / "forms" / pathFromUtf8(name + ".tvf");
        if (!within(path, p.directory))
            throw std::runtime_error("Форма вне каталога проекта");
        auto f = Form::load(path);
        if (f.name != name)
            throw std::runtime_error("Имя формы не совпадает с именем файла: " + name);
        p.forms.push_back(std::move(f));
    }
    p.openFiles = stringArray(get(j, "open_files"));
    auto &b = get(j, "build");
    p.build.cmake = str(b, "cmake", "cmake");
    p.build.directory = str(b, "directory", "build");
    p.build.generator = str(b, "generator");
    p.build.compiler = str(b, "compiler");
    p.build.profile = str(b, "profile", "Debug");
    if (p.build.profile == "debug" || p.build.profile == "dev" || p.build.profile.empty())
        p.build.profile = "Debug";
    if (p.build.profile == "release")
        p.build.profile = "Release";
    p.build.target = str(b, "target");
    p.build.extra = str(b, "extra");
    p.build.runArguments = str(b, "run_args");
    p.build.executable = str(b, "executable");
    p.build.jobs = readInt(b, "jobs", 0, 0, 1024);
    return p;
}
Json Project::toJson() const
{
    Json j = extra;
    put(j, "format", Json::string("tvstudio-project"));
    put(j, "version", Json::number("1"));
    put(j, "name", Json::string(name));
    put(j, "kind", Json::string(plainCpp ? "cpp" : "tvision-cpp"));
    put(j, "language", Json::string("cpp"));
    put(j, "tvision_path", Json::string(utf8(library)));
    put(j, "app", app.toJson());
    std::vector<std::string> names;
    for (auto &f : forms)
        names.push_back(f.name);
    put(j, "forms", strings(names));
    put(j, "open_files", strings(openFiles));
    Json b = Json::record();
    for (auto p : {std::pair<const char *, std::string>{"cmake", build.cmake},
                   {"directory", build.directory},
                   {"generator", build.generator},
                   {"compiler", build.compiler},
                   {"profile", build.profile},
                   {"target", build.target},
                   {"extra", build.extra},
                   {"run_args", build.runArguments},
                   {"executable", build.executable}})
        put(b, p.first, Json::string(p.second));
    put(b, "jobs", Json::number(std::to_string(build.jobs)));
    put(j, "build", std::move(b));
    return j;
}
void Project::save() const
{
    if (file.empty())
        throw std::runtime_error("Путь проекта не задан");
    for (auto &f : forms)
    {
        if (!identifier(f.name))
            throw std::runtime_error("Неверное имя формы");
        auto path = directory / "forms" / pathFromUtf8(f.name + ".tvf");
        if (!within(path, directory))
            throw std::runtime_error("Форма вне каталога проекта");
        f.save(path);
    }
    writeFile(file, writeYaml(toJson()));
}
int Project::formIndex(const std::string &name) const
{
    for (size_t i = 0; i < forms.size(); ++i)
        if (forms[i].name == name)
            return int(i);
    return -1;
}
std::vector<std::string> Project::validate() const
{
    std::vector<std::string> out;
    if (!projectName(name))
        out.push_back("Неверное имя проекта");
    if (plainCpp)
        return out;
    std::set<std::string> names;
    for (auto &f : forms)
    {
        auto canonical = f.name;
        std::transform(canonical.begin(), canonical.end(), canonical.begin(),
                       [](unsigned char c) { return char(std::tolower(c)); });
        if (!names.insert(canonical).second)
            out.push_back("Повторная форма: " + f.name);
        auto issues = f.validate();
        out.insert(out.end(), issues.begin(), issues.end());
    }
    if (!app.startup.empty() && formIndex(app.startup) < 0)
        out.push_back("Не найдена стартовая форма: " + app.startup);
    std::set<unsigned> values;
    names.clear();
    for (auto &c : app.commands)
    {
        if (!identifier(c.name) || !names.insert(c.name).second)
            out.push_back("Неверная или повторная команда: " + c.name);
        if (c.value < 100 || c.value >= 0x7000 || !values.insert(c.value).second)
            out.push_back("Неверный или повторный код команды: " + c.name);
        if (c.action.compare(0, 5, "form:") == 0 && formIndex(trim(c.action.substr(5))) < 0)
            out.push_back("Не найдена форма команды: " + c.name);
    }
    return out;
}
std::vector<fs::path> Project::sourceFiles() const
{
    std::vector<fs::path> files;
    std::error_code ec;
    fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied,
                                        ec),
        end;
    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }
        auto name = utf8(it->path().filename());
        if (it->is_directory(ec))
        {
            if (name == ".git" || name == "build" || name == ".tvstudio" || name == "CMakeFiles" ||
                name == "target" || name.rfind("cmake-build-", 0) == 0)
                it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec))
            continue;
        auto ext = utf8(it->path().extension());
        if (ext == ".cpp" || ext == ".cc" || ext == ".c" || ext == ".h" || ext == ".hpp" ||
            ext == ".cxx" || ext == ".hxx" || ext == ".cmake" || ext == ".json" || ext == ".yaml" ||
            ext == ".yml" || ext == ".md" || name == "CMakeLists.txt")
        {
            files.push_back(it->path());
            if (files.size() >= 20000)
                break;
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}
fs::path Project::buildDirectory() const
{
    auto path = pathFromUtf8(build.directory.empty() ? "build" : build.directory);
    return (path.is_absolute() ? path : directory / path).lexically_normal();
}
fs::path Project::executablePath() const
{
    if (!build.executable.empty())
    {
        auto p = pathFromUtf8(build.executable);
        return p.is_absolute() ? p : directory / p;
    }
    auto name = build.target.empty() ? this->name : build.target;
#ifdef _WIN32
    name += ".exe";
#endif
    for (auto p : {buildDirectory() / build.profile / pathFromUtf8(name),
                   buildDirectory() / pathFromUtf8(name)})
        if (fs::is_regular_file(p))
            return p;
    return buildDirectory() / pathFromUtf8(name);
}
FormDocument::FormDocument(Form value) : form(std::move(value))
{
    markSaved();
}
bool FormDocument::dirty() const
{
    return tvision::writeJson(form.toJson()) != saved_;
}
void FormDocument::markSaved()
{
    saved_ = tvision::writeJson(form.toJson());
}
void FormDocument::checkpoint()
{
    undo_.push_back(form);
    redo_.clear();
}
bool FormDocument::undo()
{
    if (undo_.empty())
        return false;
    redo_.push_back(form);
    form = std::move(undo_.back());
    undo_.pop_back();
    selection.clear();
    return true;
}
bool FormDocument::redo()
{
    if (redo_.empty())
        return false;
    undo_.push_back(form);
    form = std::move(redo_.back());
    redo_.pop_back();
    selection.clear();
    return true;
}
size_t FormDocument::add(Kind k, int x, int y)
{
    checkpoint();
    auto c = Control::make(k, form.uniqueName(info(k).prefix), x, y);
    c.w = std::min(c.w, std::max(1, form.w - 2));
    c.h = std::min(c.h, std::max(1, form.h - 2));
    c.x = std::max(1, std::min(c.x, form.w - 1 - c.w));
    c.y = std::max(1, std::min(c.y, form.h - 1 - c.h));
    form.controls.push_back(std::move(c));
    selection = {form.controls.size() - 1};
    return form.controls.size() - 1;
}
void FormDocument::erase()
{
    if (selection.empty())
        return;
    checkpoint();
    std::set<std::string> removed;
    for (auto i : selection)
        if (i < form.controls.size())
            removed.insert(form.controls[i].name);
    for (auto i = selection.rbegin(); i != selection.rend(); ++i)
        if (*i < form.controls.size())
            form.controls.erase(form.controls.begin() + std::ptrdiff_t(*i));
    for (auto &c : form.controls)
        if (removed.count(c.link))
            c.link.clear();
    selection.clear();
}
void FormDocument::move(int dx, int dy, bool resize)
{
    if (selection.empty() || (dx == 0 && dy == 0))
        return;
    checkpoint();
    for (auto i : selection)
        if (i < form.controls.size())
        {
            auto &c = form.controls[i];
            if (resize)
            {
                c.w = std::max<int64_t>(1, std::min<int64_t>(form.w - 1 - c.x, int64_t(c.w) + dx));
                c.h = std::max<int64_t>(1, std::min<int64_t>(form.h - 1 - c.y, int64_t(c.h) + dy));
            }
            else
            {
                c.x = std::max<int64_t>(1, std::min<int64_t>(form.w - 1 - c.w, int64_t(c.x) + dx));
                c.y = std::max<int64_t>(1, std::min<int64_t>(form.h - 1 - c.h, int64_t(c.y) + dy));
            }
        }
}
void FormDocument::selectAll()
{
    selection.clear();
    for (size_t i = 0; i < form.controls.size(); ++i)
        selection.insert(i);
}
void FormDocument::copy()
{
    clipboard_.clear();
    for (auto i : selection)
        if (i < form.controls.size())
            clipboard_.push_back(form.controls[i]);
}
void FormDocument::cut()
{
    copy();
    erase();
}
void FormDocument::paste(int dx, int dy)
{
    if (clipboard_.empty())
        return;
    checkpoint();
    selection.clear();
    std::map<std::string, std::string> renamed;
    size_t first = form.controls.size();
    for (auto c : clipboard_)
    {
        auto old = c.name;
        c.name = form.uniqueName(info(c.kind).prefix);
        renamed[old] = c.name;
        c.x = std::max<int64_t>(1, std::min<int64_t>(form.w - 1 - c.w, int64_t(c.x) + dx));
        c.y = std::max<int64_t>(1, std::min<int64_t>(form.h - 1 - c.h, int64_t(c.y) + dy));
        selection.insert(form.controls.size());
        form.controls.push_back(std::move(c));
    }
    for (size_t i = first; i < form.controls.size(); ++i)
    {
        auto &link = form.controls[i].link;
        auto p = renamed.find(link);
        if (p != renamed.end())
            link = p->second;
    }
}
void FormDocument::align(const std::string &direction)
{
    if (selection.size() < 2)
        return;
    checkpoint();
    std::vector<Control *> selected;
    for (auto i : selection)
        if (i < form.controls.size())
            selected.push_back(&form.controls[i]);
    if (selected.empty())
        return;
    auto base = *selected.front();
    int left = base.x, right = base.x + base.w, top = base.y, bottom = base.y + base.h;
    for (auto *c : selected)
    {
        left = std::min(left, c->x);
        right = std::max(right, c->x + c->w);
        top = std::min(top, c->y);
        bottom = std::max(bottom, c->y + c->h);
    }
    for (auto *c : selected)
    {
        if (direction == "left")
            c->x = left;
        else if (direction == "right")
            c->x = right - c->w;
        else if (direction == "top")
            c->y = top;
        else if (direction == "bottom")
            c->y = bottom - c->h;
        else if (direction == "center_x")
            c->x = left + (right - left - c->w) / 2;
        else if (direction == "center_y")
            c->y = top + (bottom - top - c->h) / 2;
        else if (direction == "width")
            c->w = base.w;
        else if (direction == "height")
            c->h = base.h;
    }
}
void FormDocument::tabOrder(int direction)
{
    if (selection.empty())
        return;
    checkpoint();
    std::set<std::string> names;
    for (auto i : selection)
        if (i < form.controls.size())
            names.insert(form.controls[i].name);
    if (direction < 0)
    {
        for (size_t i = 1; i < form.controls.size(); ++i)
            if (names.count(form.controls[i].name) && !names.count(form.controls[i - 1].name))
                std::swap(form.controls[i], form.controls[i - 1]);
    }
    else
    {
        for (size_t i = form.controls.size(); i > 1; --i)
            if (names.count(form.controls[i - 2].name) && !names.count(form.controls[i - 1].name))
                std::swap(form.controls[i - 2], form.controls[i - 1]);
    }
    selection.clear();
    for (size_t i = 0; i < form.controls.size(); ++i)
        if (names.count(form.controls[i].name))
            selection.insert(i);
}
std::string FormDocument::clipboardJson() const
{
    Json a = Json::list();
    for (auto &c : clipboard_)
        a.array.push_back(c.toJson());
    return tvision::writeJson(
        Json::record({{"format", Json::string("tvstudio-controls")}, {"controls", std::move(a)}}));
}
void FormDocument::setClipboardJson(const std::string &text)
{
    if (text.size() > 1024 * 1024)
        throw std::runtime_error("Слишком большой буфер элементов");
    auto j = tvision::parseJson(text);
    if (str(j, "format") != "tvstudio-controls")
        throw std::runtime_error("Буфер не содержит элементов TV Studio");
    std::vector<Control> next;
    for (auto &c : get(j, "controls").array)
        next.push_back(Control::fromJson(c));
    clipboard_ = std::move(next);
}
} // namespace tvstudio
