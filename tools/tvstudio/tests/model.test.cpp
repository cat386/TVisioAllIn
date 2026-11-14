#include <chrono>
#include <limits>
#include <thread>
#include <tvstudio/codegen.h>
#include <tvstudio/lsp.h>
#include <tvstudio/model.h>
#include <tvstudio/process.h>
#include <tvstudio/runtime.h>
#include <tvtest.h>
using namespace tvstudio;
namespace
{
template <class F> bool rejects(F f)
{
    try
    {
        f();
        return false;
    }
    catch (const std::exception &)
    {
        return true;
    }
}
fs::path temp()
{
    auto p =
        fs::temp_directory_path() /
        pathFromUtf8("tvstudio-тест " +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(p);
    return p;
}
} // namespace
TEST(StudioLsp, UnicodeCoordinatesAndEdits)
{
    std::string text = "аб😀x\nz";
    auto pos = textPosition(text, 8);
    EXPECT_EQ(integer(pos, "character"), 4);
    EXPECT_EQ(textOffset(text, pos), 8u);
    EXPECT_EQ(integer(textPosition(text, 8, "utf-8"), "character"), 8);
    EXPECT_EQ(integer(textPosition(text, 8, "utf-32"), "character"), 3);
    EXPECT_EQ(textOffset(text, textPosition(text, text.size())), text.size());
    auto edit = Json::record({{"range", Json::record({{"start", textPosition(text, 4)},
                                                      {"end", textPosition(text, 8)}})},
                              {"newText", Json::string("R")}});
    EXPECT_EQ(applyTextEdits(text, Json::list({edit})), "абRx\nz");
    EXPECT_TRUE(rejects([&] { applyTextEdits(text, Json::list({edit, edit})); }));
}
TEST(StudioLsp, FramingHandlesPartialHeadersBodiesAndMultipleMessages)
{
    auto value = Json::record({{"text", Json::string("Привет")}});
    auto text = tvision::writeJson(value);
    auto message = "Content-Length: " + std::to_string(text.size()) + "\r\n\r\n" + text;
    LspFramer reader;
    std::vector<Json> out;
    for (char c : message)
    {
        auto result = reader.feed(std::string(1, c));
        out.insert(out.end(), result.begin(), result.end());
    }
    EXPECT_EQ(out.size(), 1u);
    EXPECT_EQ(tvision::writeJson(out[0]), text);
    EXPECT_EQ(reader.feed(message + message).size(), 2u);
    EXPECT_TRUE(rejects(
        []
        {
            LspFramer f;
            f.feed("Content-Length: 20000000\r\n\r\n");
        }));
}
TEST(StudioLsp, FileUrisRoundTripUnicodeAndReservedCharacters)
{
    auto file = fs::absolute(pathFromUtf8("папка с пробелом/#file%.cpp")).lexically_normal();
    EXPECT_EQ(uriFile(fileUri(file)), file);
    EXPECT_TRUE(rejects([] { uriFile("https://example.com"); }));
}
TEST(StudioLsp, ServerInitializesAndAnswersAfterFullDocumentSync)
{
    auto dir = temp();
    auto project = Project::create("lsp", dir, dir, true);
    LspClient client;
    client.start(project, displayCommand({TVSTUDIO_PROCESS_FIXTURE, "--lsp"}));
    client.query("completion", dir / "main.cpp", "int main(){}", 3);
    bool found = false;
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!found && std::chrono::steady_clock::now() < end)
    {
        for (auto &event : client.poll())
            if (event.tag == "completion")
            {
                found = true;
                EXPECT_FALSE(event.error);
                EXPECT_EQ(str(event.result.array[0], "label"), "fixture");
            }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(found);
    client.stop();
    fs::remove_all(dir);
}
TEST(StudioYaml, ReadsNestedFormsAndInlineStrings)
{
    auto j = parseDocument(
        "format: tvstudio-form\nversion: 1\nname: test\ntitle: 'Привет: мир # текст'\ncontrols:\n  "
        "- kind: input_line\n    name: name\n    items: [one, 'два, три', \"four\\nline\"]\n  - "
        "kind: button\n    name: ok\n    default: true\n");
    auto f = Form::fromJson(j);
    EXPECT_EQ(f.controls.size(), 2u);
    EXPECT_EQ(f.title, "Привет: мир # текст");
    EXPECT_EQ(f.controls[0].items[1], "два, три");
    EXPECT_TRUE(f.controls[1].defaultButton);
    EXPECT_EQ(tvision::writeJson(parseDocument(writeYaml(j))), tvision::writeJson(j));
}
TEST(StudioYaml, LiteralAndFoldedBlocks)
{
    auto j = parseDocument("text: |-\n  Hello # literal\n  World\nnext: >\n  first\n  second\n");
    EXPECT_EQ(str(j, "text"), "Hello # literal\nWorld");
    EXPECT_EQ(str(j, "next"), "first second\n");
}
TEST(StudioYaml, RejectsDuplicatesAndMalformedCollections)
{
    EXPECT_TRUE(rejects([] { parseDocument("a: 1\na: 2\n"); }));
    EXPECT_TRUE(rejects([] { parseDocument("a: [1,2\n"); }));
    EXPECT_TRUE(rejects([] { parseDocument("a:\n\tb: 2\n"); }));
    EXPECT_TRUE(rejects([] { parseDocument("a: *unknown\n"); }));
}
TEST(StudioYaml, JsonCompatibilityAndEmptyCollections)
{
    auto j = parseDocument("{\"forms\":[],\"build\":{},\"large\":9007199254740993}");
    EXPECT_EQ(tvision::writeJson(parseDocument(writeYaml(j))), tvision::writeJson(j));
}
TEST(StudioModel, AllFourteenKindsRoundTrip)
{
    EXPECT_EQ(kinds().size(), 14u);
    for (auto &k : kinds())
    {
        auto c = Control::make(k.kind, std::string(k.prefix) + "1");
        auto j = c.toJson();
        EXPECT_EQ(tvision::writeJson(Control::fromJson(parseDocument(writeYaml(j))).toJson()),
                  tvision::writeJson(j));
    }
}
TEST(StudioModel, RejectsGeneratedNameCollisions)
{
    Form form;
    form.name = "forms";
    EXPECT_FALSE(form.validate().empty());
    form.name = "entry";
    form.controls.push_back(Control::make(Kind::Input, "Data"));
    EXPECT_FALSE(form.validate().empty());
    auto dir = temp();
    auto p = Project::create("demo", dir, dir);
    auto copy = p.forms.front();
    copy.name = "ABOUT";
    p.forms.push_back(copy);
    EXPECT_FALSE(p.validate().empty());
    EXPECT_TRUE(rejects([&] { Project::create("tvision", dir, dir); }));
    fs::remove_all(dir);
}
TEST(StudioDocument, MovementClampsBeforeIntegerOverflow)
{
    FormDocument document;
    document.add(Kind::Input);
    document.move(std::numeric_limits<int>::max(), std::numeric_limits<int>::min());
    EXPECT_TRUE(document.form.validate().empty());
    document.move(std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), true);
    EXPECT_TRUE(document.form.validate().empty());
}
TEST(StudioYaml, BomAndDepthLimits)
{
    EXPECT_EQ(str(parseDocument("\xEF\xBB\xBF{\"title\":\"ok\"}"), "title"), "ok");
    std::string nested = "a: " + std::string(200, '[') + "1" + std::string(200, ']');
    EXPECT_TRUE(rejects([&] { parseDocument(nested); }));
    EXPECT_TRUE(rejects([] { Form::fromJson(parseDocument("name: test\nclass: Unknown\n")); }));
}
TEST(StudioModel, IdentifiersAndCppLiteralSafety)
{
    EXPECT_TRUE(identifier("field_1"));
    EXPECT_FALSE(identifier("class"));
    EXPECT_FALSE(identifier("__reserved"));
    EXPECT_FALSE(identifier("a-b"));
    EXPECT_EQ(cppName("a-b"), "a_b");
    EXPECT_EQ(quotedCpp(std::string("a\0b", 3)), "\"a\\000b\"");
    EXPECT_EQ(quotedCpp("Привет\n\""), "\"Привет\\n\\\"\"");
}
TEST(StudioModel, ValidationFindsBadLinksBoundsAndNames)
{
    Form f;
    f.name = "class";
    auto c = Control::make(Kind::Label, "label1");
    c.link = "missing";
    c.x = -1;
    f.controls = {c, c};
    EXPECT_TRUE(f.validate().size() >= 4);
}
TEST(StudioModel, UnknownPropertiesArePreserved)
{
    auto j = parseDocument("kind: button\nname: ok\nfuture_property: [1, 2]\n");
    auto c = Control::fromJson(j);
    EXPECT_EQ(tvision::writeJson(get(c.toJson(), "future_property")), "[1,2]");
}
TEST(StudioDocument, AddMoveDeleteUndoRedo)
{
    FormDocument d;
    auto a = d.add(Kind::Input, 3, 3);
    EXPECT_TRUE(d.dirty());
    d.move(2, 1);
    EXPECT_EQ(d.form.controls[a].x, 5);
    d.erase();
    EXPECT_EQ(d.form.controls.size(), 0u);
    EXPECT_TRUE(d.undo());
    EXPECT_EQ(d.form.controls.size(), 1u);
    EXPECT_TRUE(d.undo());
    EXPECT_EQ(d.form.controls[a].x, 3);
    EXPECT_TRUE(d.redo());
    EXPECT_EQ(d.form.controls[a].x, 5);
    d.markSaved();
    EXPECT_FALSE(d.dirty());
}
TEST(StudioDocument, ClipboardRenamesBoundLabels)
{
    FormDocument d;
    d.add(Kind::Input);
    d.add(Kind::Label);
    d.form.controls[1].link = d.form.controls[0].name;
    d.selectAll();
    d.copy();
    auto data = d.clipboardJson();
    d.setClipboardJson(data);
    d.paste();
    EXPECT_EQ(d.form.controls.size(), 4u);
    EXPECT_EQ(d.form.controls[3].link, d.form.controls[2].name);
    EXPECT_TRUE(d.form.controls[2].name != d.form.controls[0].name);
}
TEST(StudioDocument, AlignmentAndTabOrderPreserveSelection)
{
    FormDocument d;
    d.add(Kind::Button, 2, 2);
    d.add(Kind::Button, 20, 5);
    d.selectAll();
    d.align("left");
    EXPECT_EQ(d.form.controls[0].x, d.form.controls[1].x);
    auto name = d.form.controls[1].name;
    d.selection = {1};
    d.tabOrder(-1);
    EXPECT_EQ(d.form.controls[0].name, name);
    EXPECT_EQ(*d.selection.begin(), 0u);
}
TEST(StudioProject, SavesUnicodePathsAndReloadsWithoutDataLoss)
{
    auto dir = temp();
    auto p = Project::create("demo", dir, dir);
    p.save();
    auto q = Project::load(dir);
    EXPECT_EQ(q.forms.size(), 1u);
    EXPECT_EQ(q.name, "demo");
    EXPECT_EQ(q.forms[0].controls.size(), 2u);
    q.save();
    EXPECT_TRUE(fs::exists(pathFromUtf8(utf8(q.file) + ".bak")));
    fs::remove_all(dir);
}
TEST(StudioProject, RejectsFormPathTraversal)
{
    auto dir = temp();
    writeFile(dir / "bad.tvsp", "format: tvstudio-project\nname: demo\nforms: [../outside]\n",
              false);
    EXPECT_TRUE(rejects([&] { Project::load(dir); }));
    fs::remove_all(dir);
}
TEST(StudioProject, CmakeDatabaseUsesConfiguredBuildDirectory)
{
    auto dir = temp();
    auto p = Project::create("cmake_paths", dir, dir, true);
    p.build.directory = "out/debug with spaces";
    p.build.generator = "Ninja";
    p.build.compiler = "custom-c++";
    p.build.extra = "-DCMAKE_EXPORT_COMPILE_COMMANDS=OFF -DFEATURE=ON";
    p.save();
    auto q = Project::load(p.file);
    EXPECT_EQ(q.buildDirectory(), dir / "out/debug with spaces");
    auto commands = buildCommands(q, "configure");
    EXPECT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands[0][4], utf8(q.buildDirectory()));
    EXPECT_EQ(commands[0].back(), "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON");
    EXPECT_EQ(buildCommands(q, "build")[1][2], utf8(q.buildDirectory()));
    q.build.directory = utf8(dir / "absolute");
    EXPECT_EQ(q.buildDirectory(), dir / "absolute");
    fs::remove_all(dir);
}
TEST(StudioProject, ExistingCmakeProjectIsNotOverwrittenOnOpen)
{
    auto dir = temp();
    writeFile(dir / "CMakeLists.txt", "project(existing)\n", false);
    auto p = Project::load(dir);
    EXPECT_TRUE(p.plainCpp);
    EXPECT_EQ(readFile(dir / "CMakeLists.txt"), "project(existing)\n");
    EXPECT_FALSE(fs::exists(p.file));
    fs::remove_all(dir);
}
TEST(StudioRuntime, BuildsAllWidgetsAndReadsTypedData)
{
    Form f;
    f.w = 110;
    f.h = 80;
    int y = 1;
    for (auto &k : kinds())
    {
        auto c = Control::make(k.kind, std::string(k.prefix) + "1", 2, y);
        y += c.h + 1;
        if (k.kind == Kind::Label)
            c.link = "input1";
        f.controls.push_back(c);
    }
    auto *w = makeForm(f);
    auto data = readForm(*w);
    EXPECT_EQ(str(data, "input1"), "");
    EXPECT_EQ(integer(data, "progress1"), 40);
    EXPECT_EQ(get(data, "table1").array.size(), 2u);
    EXPECT_EQ(str(data, "combo1"), "Один");
    TObject::destroy(w);
}
TEST(StudioRuntime, WindowUsesDialogPaletteForInputs)
{
    Form f;
    f.window = true;
    auto c = Control::make(Kind::Input, "name");
    c.text = "Привет";
    f.controls.push_back(c);
    auto *w = makeForm(f);
    EXPECT_EQ((uchar)w->getPalette()[0], 32);
    EXPECT_EQ(str(readForm(*w), "name"), "Привет");
    TObject::destroy(w);
}
TEST(StudioRuntime, KeysAndCommandsAreValidated)
{
    Application app;
    app.commands.push_back({"CUSTOM", "", "", 123});
    EXPECT_EQ(commandCode(app, "CUSTOM"), 123);
    EXPECT_EQ(commandCode(app, "cm::QUIT"), cmQuit);
    EXPECT_EQ(parseKey("Ctrl+Shift+F9"), TKey(kbF9, kbCtrlShift | kbShift));
    EXPECT_TRUE(rejects([&] { commandCode(app, "missing"); }));
    EXPECT_TRUE(rejects([] { parseKey("Meta+X"); }));
}
TEST(StudioRuntime, RangeValidatorSupportsInt64)
{
    Form f;
    auto c = Control::make(Kind::Input, "large");
    c.validator = "range:9007199254740992..9223372036854775807";
    c.text = "9007199254740993";
    f.controls.push_back(c);
    auto *w = makeForm(f);
    EXPECT_TRUE(w->valid(cmOK));
    TObject::destroy(w);
}
TEST(StudioCodegen, DoesNotOverwriteUserFiles)
{
    auto dir = temp();
    auto p = Project::create("demo", dir, dir, true);
    generate(p);
    writeFile(dir / "src/main.cpp", "// Пользовательский файл\nint main(){return 7;}\n", false);
    generate(p);
    EXPECT_TRUE(readFile(dir / "src/main.cpp").find("return 7") != std::string::npos);
    writeFile(dir / "CMakeLists.txt", "project(custom)\n", false);
    generate(p);
    EXPECT_EQ(readFile(dir / "CMakeLists.txt"), "project(custom)\n");
    fs::remove_all(dir);
}
TEST(StudioCodegen, ImportCopiesSourcesAndKeepsOriginal)
{
    auto from = temp(), to = temp();
    writeFile(from / "main.cpp", "int main(){return 0;}\n", false);
    writeFile(from / "header.h", "#pragma once\n", false);
    auto p = importProject(from / "main.cpp", to, "copied", from);
    EXPECT_EQ(readFile(to / "src/main.cpp"), readFile(from / "main.cpp"));
    EXPECT_TRUE(fs::exists(to / "src/header.h"));
    EXPECT_TRUE(fs::exists(p.file));
    fs::remove_all(from);
    fs::remove_all(to);
}
TEST(StudioProcess, PipesUnicodeAndQuotedArgumentsWithoutShell)
{
    auto dir = temp();
    Process p;
    std::string output, error;
    p.start(
        {TVSTUDIO_PROCESS_FIXTURE, "two words", "quote\"slash\\", "Кириллица"}, dir,
        [&](auto &s) { output += s; }, [&](auto &s) { error += s; });
    p.write("Ввод\n");
    EXPECT_EQ(p.wait(), 0);
    EXPECT_EQ(joinLines(splitLines(output)) + '\n', "Ввод\ntwo words\nquote\"slash\\\nКириллица\n");
    EXPECT_TRUE(error.find("Привет") != std::string::npos);
    fs::remove_all(dir);
}
TEST(StudioProcess, CancellationTerminatesTheChild)
{
    Process p;
    p.start({TVSTUDIO_PROCESS_FIXTURE, "--sleep"}, fs::current_path());
    p.cancel();
    EXPECT_TRUE(p.wait() != 0);
    EXPECT_TRUE(p.done());
}
TEST(StudioProcess, ParsesGccAndMsvcDiagnostics)
{
    Diagnostic d;
    EXPECT_TRUE(
        parseDiagnostic("C:/path with space/main.cpp:12:3: error: bad", fs::current_path(), d));
    EXPECT_EQ(d.line, 12);
    EXPECT_EQ(d.column, 3);
    EXPECT_TRUE(
        parseDiagnostic("C:\\src\\main.cpp(7,4): warning C1234: message", fs::current_path(), d));
    EXPECT_EQ(d.severity, 2);
    EXPECT_EQ(d.message, "message");
}
TEST(StudioProcess, ArgumentsNeverBecomeShellOperators)
{
    auto args = splitArguments("-DNAME=\"two words\" 'x; & y' C:\\source");
    EXPECT_EQ(args.size(), 3u);
    EXPECT_EQ(args[0], "-DNAME=two words");
    EXPECT_EQ(args[1], "x; & y");
    EXPECT_EQ(args[2], "C:\\source");
    EXPECT_TRUE(rejects([] { splitArguments("\"missing"); }));
}
