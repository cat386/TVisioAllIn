#include <chrono>
#include <iostream>
#include <thread>
#include <tvstudio/lsp.h>
using namespace tvstudio;
int main(int argc, char **argv)
{
    auto args = commandLineArguments(argc, argv);
    if (args.size() != 2 && args.size() != 5)
        return 2;
    auto dir = fs::temp_directory_path() /
        ("tvstudio-clangd-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    LspClient client;
    try
    {
        auto project = Project::create("lspcheck", dir, dir, true);
        auto file = dir / "main.cpp";
        std::string good = "int answer = 42;\nint main() { return answer; }\n";
        if (args.size() == 5)
        {
            project.build.directory = "out/lsp config";
            project.build.cmake = args[2];
            project.build.generator = "Ninja";
            project.build.compiler = args[4];
            project.build.extra = displayCommand({"-DCMAKE_MAKE_PROGRAM=" + args[3]});
            writeFile(dir / "CMakeLists.txt",
                "cmake_minimum_required(VERSION 3.16)\nproject(lspcheck LANGUAGES CXX)\n"
                "add_executable(lspcheck main.cpp)\n"
                "target_compile_features(lspcheck PRIVATE cxx_std_17)\n"
                "target_compile_definitions(lspcheck PRIVATE CMAKE_LSP_VALUE=42)\n"
                "configure_file(config.h.in generated/config.h @ONLY)\n"
                "target_include_directories(lspcheck PRIVATE include ${CMAKE_CURRENT_BINARY_DIR}/generated)\n", false);
            writeFile(dir / "config.h.in", "#define CONFIGURED_VALUE 42\n", false);
            writeFile(dir / "include/project_header.h",
                      "static_assert(CMAKE_LSP_VALUE == CONFIGURED_VALUE);\n", false);
            good = "#include <config.h>\n#include <project_header.h>\n" + good;
        }
        writeFile(file, good, false);
        if (args.size() == 5)
        {
            BuildRunner configure;
            configure.start(project, "configure");
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            while (configure.active() && std::chrono::steady_clock::now() < deadline)
            {
                configure.poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (configure.active() || configure.result() != 0)
            {
                for (auto &line : configure.takeOutput())
                    std::cerr << line;
                throw std::runtime_error("CMake configure failed");
            }
            if (!fs::is_regular_file(project.buildDirectory() / "compile_commands.json"))
                throw std::runtime_error("CMake compilation database missing");
        }
        client.start(project, args[1]);
        auto wait = [&](const std::string &tag, bool nonempty)
        {
            auto end = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (std::chrono::steady_clock::now() < end)
            {
                for (auto &event : client.poll())
                {
                    if (event.error)
                        throw std::runtime_error(event.result.display());
                    if (event.tag == tag)
                    {
                        auto value = tag == "completion" ? get(event.result, "items") : event.result;
                        if (value.kind == Json::Kind::Null)
                            value = event.result;
                        if (nonempty ? !value.array.empty() : value.array.empty())
                            return;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            throw std::runtime_error("Timeout: " + tag);
        };
        // Проверяем диагностику несохранённого буфера и её очистку после исправления.
        client.sync(file, "int main() { return unknown_symbol; }\n");
        wait("diagnostics", true);
        client.sync(file, good);
        wait("diagnostics", false);
        client.query("definition", file, good, good.rfind("answer") + 2);
        wait("definition", true);
        std::string completion = "int answer = 42;\nint main() { return ans";
        client.query("completion", file, completion, completion.size());
        wait("completion", true);
        client.close(file);
        client.sync(file, good);
        wait("diagnostics", false);
        client.stop();
        fs::remove_all(dir);
        std::cout << "clangd: diagnostics, unsaved changes, definition, completion, reopen OK\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        client.stop();
        fs::remove_all(dir);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
