#ifndef TVSTUDIO_LSP_H
#define TVSTUDIO_LSP_H
#include <tvstudio/process.h>

namespace tvstudio
{
std::string fileUri(const fs::path &path);
fs::path uriFile(const std::string &uri);
Json textPosition(const std::string &text, size_t offset, const std::string &encoding = "utf-16");
size_t textOffset(const std::string &text, const Json &position,
                  const std::string &encoding = "utf-16");
std::string applyTextEdits(const std::string &text, const Json &edits,
                           const std::string &encoding = "utf-16");
class LspFramer
{
    std::string buffer_;

  public:
    std::vector<Json> feed(const std::string &bytes);
};
struct LspEvent
{
    std::string tag;
    fs::path file;
    std::string text;
    Json result;
    bool error = false;
};
class LspClient
{
    Process process_;
    std::mutex mutex_;
    LspFramer framer_;
    std::deque<Json> incoming_;
    std::deque<LspEvent> events_;
    std::map<int, LspEvent> pending_;
    std::map<std::string, int> versions_;
    std::vector<Json> waiting_;
    int next_ = 1;
    bool initialized_ = false, started_ = false;
    std::string encoding_ = "utf-16";
    void send(Json message);
    int request(const std::string &method, Json params, LspEvent event);
    void notify(const std::string &method, Json params);

  public:
    ~LspClient()
    {
        stop();
    }
    void start(const Project &project, const std::string &command = "clangd");
    void stop();
    bool running() const
    {
        return started_ && !process_.done();
    }
    const std::string &encoding() const
    {
        return encoding_;
    }
    void query(const std::string &method, const fs::path &file, const std::string &text,
               size_t cursor);
    std::vector<LspEvent> poll();
};
} // namespace tvstudio
#endif
