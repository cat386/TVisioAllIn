#ifndef TVSTUDIO_PROCESS_H
#define TVSTUDIO_PROCESS_H
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <tvstudio/model.h>

namespace tvstudio
{
std::vector<std::string> splitArguments(const std::string &text);
std::string displayCommand(const std::vector<std::string> &arguments);
std::vector<std::string> commandLineArguments(int argc, char **argv);
class Process
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    using Output = std::function<void(const std::string &)>;
    Process();
    ~Process();
    Process(const Process &) = delete;
    Process &operator=(const Process &) = delete;
    void start(const std::vector<std::string> &arguments, const fs::path &directory,
               Output output = {}, Output error = {}, bool interactive = false);
    void write(std::string data);
    void cancel();
    bool done() const;
    int exitCode() const;
    int wait();
};
struct Diagnostic
{
    fs::path file;
    int line = 1, column = 1, endLine = 1, endColumn = 1;
    int severity = 1;
    std::string message, source;
};
bool parseDiagnostic(const std::string &line, const fs::path &directory, Diagnostic &result);
std::vector<std::vector<std::string>> buildCommands(const Project &project,
                                                    const std::string &action);
class BuildRunner
{
    mutable std::mutex mutex_;
    std::deque<std::string> output_;
    std::vector<std::vector<std::string>> commands_;
    fs::path directory_;
    size_t next_ = 0;
    bool active_ = false, cancelled_ = false;
    int result_ = 0;
    Process process_;
    void launch();

  public:
    ~BuildRunner();
    void start(const Project &project, const std::string &action);
    void stop();
    void poll();
    bool active() const
    {
        return active_;
    }
    int result() const
    {
        return result_;
    }
    std::vector<std::string> takeOutput();
};
} // namespace tvstudio
#endif
