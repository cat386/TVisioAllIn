#include <chrono>
#include <iostream>
#include <thread>
#include <tvstudio/process.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main(int argc, char **argv)
{
    auto args = tvstudio::commandLineArguments(argc, argv);
    if (args.size() > 1 && args[1] == "--lsp")
    {
        using namespace tvstudio;
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (line.rfind("Content-Length: ", 0) != 0)
                continue;
            size_t size = std::stoul(line.substr(16));
            while (std::getline(std::cin, line) && line != "\r" && !line.empty())
            {
            }
            std::string body(size, '\0');
            std::cin.read(&body[0], size);
            auto message = tvision::parseJson(body);
            if (get(message, "id").kind == Json::Kind::Null)
                continue;
            auto result =
                str(message, "method") == "initialize"
                    ? Json::record({{"capabilities",
                                     Json::record({{"positionEncoding", Json::string("utf-16")}})}})
                    : Json::list({Json::record({{"label", Json::string("fixture")}})});
            auto text = tvision::writeJson(Json::record({{"jsonrpc", Json::string("2.0")},
                                                         {"id", get(message, "id")},
                                                         {"result", result}}));
            std::cout << "Content-Length: " << text.size() << "\r\n\r\n" << text << std::flush;
        }
        return 0;
    }
    if (args.size() > 1 && args[1] == "--sleep")
    {
        std::cout << "started\n" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 21;
    }
    std::string line;
    std::getline(std::cin, line);
    std::cout << line << '\n';
    for (size_t i = 1; i < args.size(); ++i)
        std::cout << args[i] << '\n';
    std::cerr << "stderr: Привет\n";
    return 0;
}
