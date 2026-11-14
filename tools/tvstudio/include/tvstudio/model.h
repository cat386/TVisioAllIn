#ifndef TVSTUDIO_MODEL_H
#define TVSTUDIO_MODEL_H
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <tvision/json.h>
#include <vector>

namespace tvstudio
{
namespace fs = std::filesystem;
using tvision::Json;

std::string utf8(const fs::path &path);
fs::path pathFromUtf8(const std::string &text);
std::string readFile(const fs::path &path, size_t limit = 32 * 1024 * 1024);
void writeFile(const fs::path &path, const std::string &text, bool backup = true);
std::string trim(std::string text);
std::vector<std::string> splitLines(const std::string &text);
std::string joinLines(const std::vector<std::string> &lines);
bool within(const fs::path &path, const fs::path &directory);
bool identifier(const std::string &text);
std::string cppName(const std::string &text);
std::string quotedCpp(const std::string &text);

const Json &get(const Json &object, const std::string &key);
std::string str(const Json &object, const std::string &key, std::string fallback = {});
int64_t integer(const Json &object, const std::string &key, int64_t fallback = 0);
bool boolean(const Json &object, const std::string &key, bool fallback = false);
void put(Json &object, std::string key, Json value);
Json parseDocument(const std::string &text);
std::string writeYaml(const Json &value);

enum class Kind
{
    Text,
    Label,
    Button,
    Input,
    Checks,
    Radios,
    List,
    Memo,
    Combo,
    Scroll,
    Progress,
    Table,
    Tree,
    Line
};
struct KindInfo
{
    Kind kind;
    const char *id, *title, *prefix;
    int width, height;
};
const std::vector<KindInfo> &kinds();
const KindInfo &info(Kind kind);
Kind parseKind(const std::string &id);

struct Rect
{
    int x = 1, y = 1, w = 12, h = 1;
    bool contains(int px, int py) const;
    bool intersects(const Rect &other) const;
};
struct Control : Rect
{
    Kind kind = Kind::Text;
    std::string name, text, command, link, validator;
    std::vector<std::string> items;
    int maxLength = 64, history = 0, columns = 1;
    int64_t value = 0, maximum = 100;
    bool defaultButton = false, scrollbar = false, header = true, grid = true;
    bool editable = false, rowNumbers = false, checks = false, lines = true, expanded = true;
    bool vertical = false, doubleLine = false, toFrame = false, disabled = false;
    int help = 0;
    Json extra = Json::record();
    static Control make(Kind kind, std::string name, int x = 2, int y = 2);
    Json toJson() const;
    static Control fromJson(const Json &json);
    bool hasData() const;
};
struct Form
{
    std::string name = "about", title = "О программе", palette = "gray";
    int x = 5, y = 2, w = 50, h = 15, number = 0, help = 0;
    bool centered = true, window = false, resizable = true;
    std::vector<Control> controls;
    Json extra = Json::record();
    std::string uniqueName(const std::string &prefix) const;
    int find(const std::string &name) const;
    int hit(int x, int y) const;
    Json toJson() const;
    static Form fromJson(const Json &json);
    static Form load(const fs::path &path);
    void save(const fs::path &path) const;
    std::vector<std::string> validate() const;
};
struct Command
{
    std::string name, action, comment;
    unsigned value = 100;
};
struct Menu
{
    std::string title, command, key;
    int help = 0;
    bool separator = false;
    std::vector<Menu> items;
};
struct Status
{
    std::string text, key, command;
};
struct Application
{
    std::string title, startup;
    std::vector<Command> commands;
    std::vector<Menu> menu;
    std::vector<Status> status;
    static Application starter(const std::string &title);
    Json toJson() const;
    static Application fromJson(const Json &json);
};
struct BuildOptions
{
    std::string directory = "build";
    std::string cmake = "cmake", generator, compiler, profile = "Debug", target;
    std::string extra, runArguments, executable;
    int jobs = 0;
};
struct Project
{
    fs::path file, directory, library;
    std::string name = "application";
    bool plainCpp = false;
    Application app;
    std::vector<Form> forms;
    std::vector<std::string> openFiles;
    BuildOptions build;
    Json extra = Json::record();
    static Project create(std::string name, const fs::path &directory, const fs::path &library,
                          bool plain = false);
    static Project load(const fs::path &path);
    Json toJson() const;
    void save() const;
    std::vector<std::string> validate() const;
    std::vector<fs::path> sourceFiles() const;
    fs::path buildDirectory() const;
    fs::path executablePath() const;
    int formIndex(const std::string &name) const;
};

// Изменения дизайнера хранятся отдельно от виджетов; отмена не зависит от их времени жизни.
class FormDocument
{
    std::vector<Form> undo_, redo_;
    std::string saved_;
    std::vector<Control> clipboard_;

  public:
    Form form;
    std::set<size_t> selection;
    explicit FormDocument(Form value = {});
    bool dirty() const;
    void markSaved();
    void checkpoint();
    bool undo();
    bool redo();
    size_t add(Kind kind, int x = 2, int y = 2);
    void erase();
    void move(int dx, int dy, bool resize = false);
    void selectAll();
    void copy();
    void cut();
    void paste(int dx = 1, int dy = 1);
    void align(const std::string &direction);
    void tabOrder(int direction);
    std::string clipboardJson() const;
    void setClipboardJson(const std::string &text);
};
} // namespace tvstudio
#endif
