#include <sstream>
#include <stdexcept>
#include <tvstudio/codegen.h>
#include <tvstudio/runtime.h>

namespace tvstudio
{
namespace
{
const std::string marker = "// Сгенерировано TV Studio C++. Изменяйте модель, а не этот файл.\n";
const std::string cmakeMarker = "# Сгенерировано TV Studio C++.\n";
std::string cmakeQuote(std::string s)
{
    std::string out = "\"";
    for (char c : s)
    {
        if (c == '\\' || c == '"' || c == '$' || c == ';')
            out += '\\';
        out += c;
    }
    return out + '"';
}
bool saveGenerated(const Project &p, const fs::path &relative, const std::string &text,
                   bool owned = true)
{
    auto path = p.directory / relative;
    if (!within(path, p.directory))
        throw std::runtime_error("Выходной файл за пределами проекта: " + utf8(path));
    if (fs::exists(path))
    {
        auto previous = readFile(path);
        if (previous == text)
            return false;
        if (!owned)
            return false;
        if (previous.rfind(marker, 0) != 0 && previous.rfind(cmakeMarker, 0) != 0)
            throw std::runtime_error("Файл не принадлежит генератору и не будет перезаписан: " +
                                     utf8(path));
    }
    writeFile(path, text, true);
    return true;
}
std::string fieldType(const Control &c)
{
    if (c.kind == Kind::Input || c.kind == Kind::Combo || c.kind == Kind::Memo)
        return "std::string";
    if (c.kind == Kind::Checks || c.kind == Kind::Radios)
        return "uint16_t";
    if (c.kind == Kind::Table)
        return "std::vector<std::vector<std::string>>";
    return "int64_t";
}
std::string initial(const Control &c)
{
    if (c.kind == Kind::Input || c.kind == Kind::Memo)
        return quotedCpp(c.text);
    if (c.kind == Kind::Combo)
        return quotedCpp(c.text.empty() && !c.items.empty() ? c.items.front() : c.text);
    if (c.kind == Kind::Table)
    {
        auto model = tableModel(c);
        std::string value = "{";
        for (auto &row : model->rows())
        {
            value += "{";
            for (auto &cell : row)
                value += quotedCpp(cell) + ",";
            value += "},";
        }
        return value + "}";
    }
    return std::to_string(
        c.kind == Kind::Checks || c.kind == Kind::Radios ? int64_t(uint16_t(c.value)) : c.value);
}
void checkCommands(const Project &p)
{
    std::function<void(const std::vector<Menu> &)> visit = [&](auto &menus)
    {
        for (auto &m : menus)
        {
            parseKey(m.key);
            if (!m.items.empty())
                visit(m.items);
            else if (!m.separator)
                commandCode(p.app, m.command);
        }
    };
    visit(p.app.menu);
    for (auto &s : p.app.status)
    {
        parseKey(s.key);
        commandCode(p.app, s.command);
    }
    for (auto &f : p.forms)
    {
        auto *view = makeForm(f, {}, p.app);
        TObject::destroy(view);
    }
}
std::string formHeader(const Form &f)
{
    std::ostringstream o;
    o << marker << "#pragma once\n#include <tvstudio/runtime.h>\nnamespace forms { namespace "
      << f.name << " {\nstruct Data {\n";
    for (auto &c : f.controls)
        if (c.hasData())
            o << "    " << fieldType(c) << " " << c.name << " = " << initial(c) << ";\n";
    o << "};\nclass View : public tvstudio::" << (f.window ? "FormWindow" : "FormDialog")
      << " {\npublic:\n    explicit View(const Data &data = {});\n    Data read() const;\n};\nView "
         "*build(const Data &data = {});\nView *open(TProgram &app, const Data &data = "
         "{});\nushort run(TProgram &app, Data &data, tvstudio::CommandHandler handler = "
         "{});\n}}\n";
    return o.str();
}
std::string formSource(const Form &f, const Application &app)
{
    std::ostringstream o;
    o << marker << "#include \"" << f.name << ".h\"\nnamespace forms { namespace " << f.name
      << " {\nusing namespace tvstudio;\nnamespace {\nForm model() { return "
         "Form::fromJson(tvision::parseJson("
      << quotedCpp(tvision::writeJson(f.toJson()))
      << ")); }\nApplication application() { return Application::fromJson(tvision::parseJson("
      << quotedCpp(tvision::writeJson(app.toJson()))
      << ")); }\nJson encode(const Data &data) {\n    Json out = Json::record();\n";
    for (auto &c : f.controls)
        if (c.hasData())
        {
            auto key = quotedCpp(c.name);
            if (c.kind == Kind::Table)
                o << "    { Json rows = Json::list(); for (const auto &row : data." << c.name
                  << ") { Json cells = Json::list(); for (const auto &cell : row) "
                     "cells.array.push_back(Json::string(cell)); "
                     "rows.array.push_back(std::move(cells)); } put(out, "
                  << key << ", std::move(rows)); }\n";
            else
                o << "    put(out, " << key << ", "
                  << (fieldType(c) == "std::string" ? "Json::string(data."
                                                    : "Json::number(std::to_string(data.")
                  << c.name << (fieldType(c) == "std::string" ? "));\n" : ")));\n");
        }
    o << "    return out;\n}\n}\nView::View(const Data &data) : TWindowInit(&View::initFrame), "
      << (f.window ? "FormWindow" : "FormDialog")
      << "(model(), encode(data), application()) {}\nData View::read() const {\n    auto json = "
         "binding.read(); Data data;\n";
    for (auto &c : f.controls)
        if (c.hasData())
        {
            auto key = quotedCpp(c.name);
            if (c.kind == Kind::Table)
                o << "    data." << c.name << ".clear(); for (const auto &row : get(json, " << key
                  << ").array) { std::vector<std::string> cells; for (const auto &cell : "
                     "row.array) cells.push_back(cell.display()); data."
                  << c.name << ".push_back(std::move(cells)); }\n";
            else
                o << "    data." << c.name << " = "
                  << (fieldType(c) == "std::string"
                          ? "str(json, "
                          : "static_cast<" + fieldType(c) + ">(integer(json, ")
                  << key << (fieldType(c) == "std::string" ? ");\n" : "));\n");
        }
    o << "    return data;\n}\nView *build(const Data &data) { return new View(data); }\nView "
         "*open(TProgram &app, const Data &data) { auto *view = build(data); if "
         "(!app.validView(view)) return nullptr; TProgram::deskTop->insert(view); return view; "
         "}\nushort run(TProgram &, Data &data, CommandHandler handler) {\n    auto *view = "
         "build(data); view->onCommand = std::move(handler);\n    try { auto result = "
         "TProgram::deskTop->execView(view); if (result != cmCancel) data = view->read(); "
         "TObject::destroy(view); return result; } catch (...) { TObject::destroy(view); throw; "
         "}\n}\n}}\n";
    return o.str();
}
std::string cmake(const Project &p)
{
    std::ostringstream o;
    o << cmakeMarker << "cmake_minimum_required(VERSION 3.16)\nproject(" << p.name
      << " LANGUAGES C CXX)\nset(CMAKE_CXX_STANDARD 17)\nset(CMAKE_CXX_STANDARD_REQUIRED "
         "ON)\nset(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n";
    if (!p.plainCpp)
    {
        o << "set(TVISION_SOURCE " << cmakeQuote(utf8(p.library))
          << " CACHE PATH \"TVisioAllIn source directory\")\nset(TV_BUILD_EXAMPLES OFF CACHE BOOL "
             "\"\" FORCE)\nset(TV_BUILD_TESTS OFF CACHE BOOL \"\" FORCE)\nset(TV_BUILD_TOOLS OFF "
             "CACHE BOOL \"\" FORCE)\nadd_subdirectory(\"${TVISION_SOURCE}\" "
             "\"${CMAKE_BINARY_DIR}/tvision\")\nset(TVSTUDIO_BUILD_IDE "
             "OFF)\nadd_subdirectory(\"${TVISION_SOURCE}/tools/tvstudio\" "
             "\"${CMAKE_BINARY_DIR}/tvstudio-runtime\")\n";
    }
    o << "file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)\nadd_executable(" << p.name
      << " ${APP_SOURCES})\n";
    if (!p.plainCpp)
        o << "target_link_libraries(" << p.name << " PRIVATE tvstudio-runtime)\n";
    o << "target_include_directories(" << p.name
      << " PRIVATE src)\nif(MSVC)\n    target_compile_options(" << p.name
      << " PRIVATE /utf-8)\nendif()\n";
    return o.str();
}
std::string mainSource(bool plain)
{
    if (plain)
        return "// Ваш код. Этот файл создаётся один раз.\n#include <iostream>\nint main() { "
               "std::cout << \"Hello, world!\\n\"; }\n";
    return R"CPP(// Ваш код. Генератор не перезаписывает main.cpp.
#include "ui.h"
class Application : public tvision::TThemedApplication {
public:
    Application() : TProgInit(&ui::statusLine, &ui::menuBar, &TApplication::initDeskTop) { ui::startup(*this); }
    void handleEvent(TEvent &event) override {
        TThemedApplication::handleEvent(event);
        if (event.what == evCommand && ui::handleCommand(*this, event.message.command)) clearEvent(event);
        // Здесь можно обрабатывать собственные команды.
    }
};
int main() {
    Application app;
    app.run();
    app.shutDown();
}
)CPP";
}
std::string uiSource(const Project &p)
{
    std::ostringstream o;
    o << marker
      << "#include \"ui.h\"\n#include \"forms/forms.h\"\nnamespace ui {\nnamespace "
         "{\ntvstudio::Application model() { return "
         "tvstudio::Application::fromJson(tvision::parseJson("
      << quotedCpp(tvision::writeJson(p.app.toJson()))
      << ")); }\n}\nTMenuBar *menuBar(TRect bounds) { return tvstudio::makeMenuBar(bounds, "
         "model()); }\nTStatusLine *statusLine(TRect bounds) { return "
         "tvstudio::makeStatusLine(bounds, model()); }\nbool handleCommand(TProgram &app, ushort "
         "command) {\n    switch (command) {\n";
    for (auto &c : p.app.commands)
    {
        o << "    case cmd::" << c.name << ": {\n";
        if (c.action.rfind("form:", 0) == 0)
        {
            auto name = trim(c.action.substr(5));
            auto fi = p.formIndex(name);
            o << "        forms::" << name << "::Data data;\n";
            if (fi >= 0 && p.forms[size_t(fi)].window)
                o << "        auto *view = forms::" << name
                  << "::open(app, data); if (view) view->onCommand = [&app](ushort c, TWindow &) { "
                     "handleCommand(app, c); };\n";
            else
                o << "        forms::" << name
                  << "::run(app, data, [&app](ushort c, TWindow &) { handleCommand(app, c); });\n";
        }
        else if (!c.action.empty())
        {
            auto text = c.action.rfind("message:", 0) == 0 ? c.action.substr(8) : c.action;
            o << "        messageBox(" << quotedCpp(text) << ", mfInformation | mfOKButton);\n";
        }
        o << "        return true;\n    }\n";
    }
    o << "    default: return false;\n    }\n}\nvoid startup(TProgram &app) {\n";
    if (!p.app.startup.empty())
    {
        auto index = p.formIndex(p.app.startup);
        o << "    forms::" << p.app.startup << "::Data data;\n";
        if (index >= 0 && p.forms[size_t(index)].window)
            o << "    forms::" << p.app.startup << "::open(app, data);\n";
        else
            o << "    forms::" << p.app.startup << "::run(app, data);\n";
    }
    else
        o << "    (void) app;\n";
    o << "}\n}\n";
    return o.str();
}
} // namespace
std::vector<GeneratedFile> generate(Project &p)
{
    auto errors = p.validate();
    if (!errors.empty())
        throw std::runtime_error(joinLines(errors));
    if (!p.plainCpp)
    {
        if (!fs::is_regular_file(p.library / "CMakeLists.txt") ||
            !fs::is_regular_file(p.library / "tools/tvstudio/include/tvstudio/runtime.h"))
            throw std::runtime_error("Укажите каталог TVisioAllIn с tools/tvstudio");
        checkCommands(p);
    }
    // Сначала готовим весь результат и проверяем принадлежность файлов; лишь затем записываем.
    std::vector<std::pair<fs::path, std::string>> pending;
    bool existingPlain = p.plainCpp && fs::exists(p.directory / "CMakeLists.txt") &&
                         readFile(p.directory / "CMakeLists.txt").rfind(cmakeMarker, 0) != 0;
    if (!existingPlain)
        pending.emplace_back("CMakeLists.txt", cmake(p));
    if (!p.plainCpp)
    {
        std::ostringstream header;
        header << marker << "#pragma once\n#include <tvstudio/runtime.h>\nnamespace cmd {\n";
        for (auto &c : p.app.commands)
        {
            if (!c.comment.empty())
            {
                for (auto line : splitLines(c.comment))
                {
                    line = trim(line);
                    if (!line.empty() && line.back() == char(92))
                        line += '.';
                    header << "// " << line << "\n";
                }
            }
            header << "constexpr ushort " << c.name << " = " << c.value << ";\n";
        }
        header << "}\nnamespace ui {\nTMenuBar *menuBar(TRect bounds);\nTStatusLine "
                  "*statusLine(TRect bounds);\nbool handleCommand(TProgram &app, ushort "
                  "command);\nvoid startup(TProgram &app);\n}\n";
        pending.emplace_back("src/ui.h", header.str());
        pending.emplace_back("src/ui.cpp", uiSource(p));
        std::string all = marker + "#pragma once\n";
        for (auto &f : p.forms)
        {
            pending.emplace_back(pathFromUtf8("src/forms/" + f.name + ".h"), formHeader(f));
            pending.emplace_back(pathFromUtf8("src/forms/" + f.name + ".cpp"),
                                 formSource(f, p.app));
            all += "#include \"" + f.name + ".h\"\n";
        }
        pending.emplace_back("src/forms/forms.h", all);
    }
    for (auto &file : pending)
    {
        auto path = p.directory / file.first;
        if (!within(path, p.directory))
            throw std::runtime_error("Выходной файл вне проекта");
        if (fs::exists(path))
        {
            auto old = readFile(path);
            if (old.rfind(marker, 0) != 0 && old.rfind(cmakeMarker, 0) != 0)
                throw std::runtime_error("Генератор не перезаписывает пользовательский файл: " +
                                         utf8(path));
        }
    }
    std::vector<GeneratedFile> out;
    for (auto &file : pending)
        out.push_back({file.first, saveGenerated(p, file.first, file.second)});
    if (!existingPlain)
        out.push_back(
            {"src/main.cpp", saveGenerated(p, "src/main.cpp", mainSource(p.plainCpp), false)});
    if (!p.plainCpp)
    {
        std::string flags = "-std=c++17\n-I" + utf8(p.library / "include") + "\n-I" +
                            utf8(p.library / "tools/tvstudio/include") + "\n-I" +
                            utf8(p.directory / "src") + "\n";
        auto path = p.directory / "compile_flags.txt";
        if (!fs::exists(path))
            writeFile(path, flags, false);
    }
    p.save();
    return out;
}
Project importProject(const fs::path &source, const fs::path &destination, const std::string &name,
                      const fs::path &library)
{
    if (fs::exists(destination) && !fs::is_empty(destination))
        throw std::runtime_error("Каталог назначения должен быть пустым");
    auto project = Project::create(name, destination, library, true);
    fs::create_directories(project.directory);
    fs::path directory = fs::is_directory(source) ? source : source.parent_path();
    if (within(project.directory, directory))
        throw std::runtime_error("Каталог импорта не должен находиться внутри исходного проекта");
    bool hasCmake = fs::is_regular_file(directory / "CMakeLists.txt");
    if (hasCmake)
    {
        for (fs::recursive_directory_iterator it(directory), end; it != end; ++it)
        {
            auto rel = fs::relative(it->path(), directory);
            auto part = utf8(it->path().filename());
            if (it->is_symlink())
            {
                if (it->is_directory())
                    it.disable_recursion_pending();
                continue;
            }
            if (it->is_directory())
            {
                if (part == ".git" || part == "build" || part == "target" ||
                    part.rfind("cmake-build-", 0) == 0)
                    it.disable_recursion_pending();
                continue;
            }
            auto target = project.directory / rel;
            if (!within(target, project.directory))
                throw std::runtime_error("Недопустимый путь импорта");
            fs::create_directories(target.parent_path());
            fs::copy_file(it->path(), target);
        }
    }
    else
    {
        auto main = fs::is_regular_file(source) ? source : directory / "main.cpp";
        if (!fs::is_regular_file(main))
            main = directory / "src/main.cpp";
        if (!fs::is_regular_file(main))
            throw std::runtime_error("Не найден исходный файл main.cpp");
        writeFile(project.directory / "src/main.cpp", readFile(main), false);
        for (auto &e : fs::directory_iterator(main.parent_path()))
            if (e.is_regular_file() && e.path() != main)
            {
                auto extension = utf8(e.path().extension());
                if (extension == ".h" || extension == ".hpp" || extension == ".cpp" ||
                    extension == ".cc")
                    writeFile(project.directory / "src" / e.path().filename(), readFile(e.path()),
                              false);
            }
    }
    project.save();
    generate(project);
    return project;
}
} // namespace tvstudio
