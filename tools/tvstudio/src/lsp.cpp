#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <tvstudio/lsp.h>

namespace tvstudio
{
namespace
{
unsigned hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    throw std::runtime_error("Неверный URI");
}
std::pair<uint32_t, size_t> codepoint(const std::string &s, size_t p)
{
    unsigned c = static_cast<unsigned char>(s[p]);
    size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    uint32_t value = c & ((1u << (7 - n)) - 1);
    if (n == 1)
        return {c, 1};
    if (p + n > s.size())
        return {0xFFFD, 1};
    for (size_t i = 1; i < n; ++i)
    {
        unsigned b = static_cast<unsigned char>(s[p + i]);
        if ((b & 0xC0) != 0x80)
            return {0xFFFD, 1};
        value = (value << 6) | (b & 63);
    }
    return {value, n};
}
size_t units(uint32_t cp, size_t bytes, const std::string &encoding)
{
    return encoding == "utf-8" ? bytes : encoding == "utf-32" ? 1 : cp > 0xFFFF ? 2 : 1;
}
Json idObject(const std::string &uri)
{
    return Json::record({{"uri", Json::string(uri)}});
}
} // namespace
std::string fileUri(const fs::path &path)
{
    auto p = utf8(fs::absolute(path).lexically_normal());
    std::replace(p.begin(), p.end(), '\\', '/');
    std::string out = p.rfind("//", 0) == 0           ? "file:"
                      : p.empty() || p.front() != '/' ? "file:///"
                                                      : "file://";
    const char *digits = "0123456789ABCDEF";
    for (unsigned char c : p)
    {
        if (std::isalnum(c) && c < 128 || c == '/' || c == ':' || c == '-' || c == '_' ||
            c == '.' || c == '~')
            out += char(c);
        else
        {
            out += '%';
            out += digits[c >> 4];
            out += digits[c & 15];
        }
    }
    return out;
}
fs::path uriFile(const std::string &uri)
{
    if (uri.rfind("file://", 0) != 0)
        throw std::runtime_error("Ожидался URI локального файла");
    std::string s = uri.substr(7), out;
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '%')
        {
            if (i + 2 >= s.size())
                throw std::runtime_error("Неверный URI");
            char c = char(hex(s[i + 1]) * 16 + hex(s[i + 2]));
            if (c == 0)
                throw std::runtime_error("Нулевой символ URI");
            out += c;
            i += 2;
        }
        else
            out += s[i];
    }
#ifdef _WIN32
    if (out.size() > 2 && out[0] == '/' && out[2] == ':')
        out.erase(0, 1);
    else if (!out.empty() && out.front() != '/')
        out = "//" + out;
#else
    if (!out.empty() && out.front() != '/')
    {
        if (out.rfind("localhost/", 0) == 0)
            out.erase(0, 9);
        else
            throw std::runtime_error("Удалённый URI не поддерживается");
    }
#endif
    return pathFromUtf8(out);
}
Json textPosition(const std::string &text, size_t offset, const std::string &encoding)
{
    size_t line = 0, column = 0;
    offset = std::min(offset, text.size());
    for (size_t p = 0; p < offset;)
    {
        auto cp = codepoint(text, p);
        if (p + cp.second > offset)
            break;
        if (text[p] == '\n')
        {
            ++line;
            column = 0;
        }
        else
            column += units(cp.first, cp.second, encoding);
        p += cp.second;
    }
    return Json::record({{"line", Json::number(std::to_string(line))},
                         {"character", Json::number(std::to_string(column))}});
}
size_t textOffset(const std::string &text, const Json &position, const std::string &encoding)
{
    auto line = integer(position, "line"), column = integer(position, "character");
    if (line < 0 || column < 0)
        throw std::runtime_error("Отрицательная позиция LSP");
    size_t p = 0;
    for (int64_t i = 0; i < line; ++i)
    {
        auto n = text.find('\n', p);
        if (n == std::string::npos)
            return text.size();
        p = n + 1;
    }
    size_t count = 0;
    while (p < text.size() && text[p] != '\n' && count < size_t(column))
    {
        auto cp = codepoint(text, p);
        auto n = units(cp.first, cp.second, encoding);
        if (count + n > size_t(column))
            break;
        count += n;
        p += cp.second;
    }
    return p;
}
std::string applyTextEdits(const std::string &text, const Json &edits, const std::string &encoding)
{
    if (edits.kind != Json::Kind::Array)
        throw std::runtime_error("Ожидался массив исправлений LSP");
    struct Edit
    {
        size_t start, end;
        std::string value;
    };
    std::vector<Edit> ranges;
    for (auto &e : edits.array)
    {
        auto &range = get(e, "range");
        size_t start = textOffset(text, get(range, "start"), encoding),
               end = textOffset(text, get(range, "end"), encoding);
        if (start > end)
            throw std::runtime_error("Обратный диапазон LSP");
        ranges.push_back({start, end, str(e, "newText")});
    }
    std::stable_sort(ranges.begin(), ranges.end(),
                     [](auto &a, auto &b) { return a.start > b.start; });
    auto out = text;
    size_t previous = text.size();
    for (auto &r : ranges)
    {
        if (r.end > previous)
            throw std::runtime_error("Пересекающиеся исправления LSP");
        out.replace(r.start, r.end - r.start, r.value);
        previous = r.start;
    }
    return out;
}
std::vector<Json> LspFramer::feed(const std::string &bytes)
{
    if (bytes.size() > 16 * 1024 * 1024 || buffer_.size() + bytes.size() > 32 * 1024 * 1024)
        throw std::runtime_error("Слишком большое сообщение LSP");
    buffer_ += bytes;
    std::vector<Json> out;
    for (;;)
    {
        auto end = buffer_.find("\r\n\r\n");
        if (end == std::string::npos)
        {
            if (buffer_.size() > 8192)
                throw std::runtime_error("Слишком длинный заголовок LSP");
            break;
        }
        size_t length = std::string::npos;
        for (auto line : splitLines(buffer_.substr(0, end)))
        {
            auto colon = line.find(':');
            if (colon == std::string::npos)
                continue;
            auto name = line.substr(0, colon);
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            if (name == "content-length")
            {
                auto value = trim(line.substr(colon + 1));
                size_t used = 0;
                auto n = std::stoull(value, &used);
                if (used != value.size() || n > 16 * 1024 * 1024 || length != std::string::npos)
                    throw std::runtime_error("Неверный Content-Length LSP");
                length = size_t(n);
            }
        }
        if (length == std::string::npos)
            throw std::runtime_error("Нет Content-Length LSP");
        if (buffer_.size() - end - 4 < length)
            break;
        out.push_back(tvision::parseJson(buffer_.substr(end + 4, length)));
        buffer_.erase(0, end + 4 + length);
    }
    return out;
}
void LspClient::send(Json message)
{
    put(message, "jsonrpc", Json::string("2.0"));
    auto text = tvision::writeJson(message);
    process_.write("Content-Length: " + std::to_string(text.size()) + "\r\n\r\n" + text);
}
int LspClient::request(const std::string &method, Json params, LspEvent event)
{
    int id = next_++;
    pending_[id] = std::move(event);
    auto message = Json::record({{"id", Json::number(std::to_string(id))},
                                 {"method", Json::string(method)},
                                 {"params", std::move(params)}});
    if (!initialized_ && method != "initialize")
        waiting_.push_back(std::move(message));
    else
        send(std::move(message));
    return id;
}
void LspClient::notify(const std::string &method, Json params)
{
    auto message = Json::record({{"method", Json::string(method)}, {"params", std::move(params)}});
    if (!initialized_ && method != "initialized")
        waiting_.push_back(std::move(message));
    else
        send(std::move(message));
}
void LspClient::start(const Project &project, const std::string &command)
{
    stop();
    auto args = splitArguments(command);
    if (args.empty())
        throw std::runtime_error("Не задан clangd");
    args.push_back("--background-index");
    args.push_back("--compile-commands-dir=" + utf8(project.buildDirectory()));
    process_.start(
        args, project.directory,
        [this](const std::string &bytes)
        {
            try
            {
                auto messages = framer_.feed(bytes);
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto &j : messages)
                    incoming_.push_back(std::move(j));
            }
            catch (const std::exception &e)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                events_.push_back({"error", {}, e.what(), {}, true});
            }
        },
        [](const std::string &) {});
    started_ = true;
    auto capabilities = tvision::parseJson(
        R"JSON({"general":{"positionEncodings":["utf-16","utf-8"]},"textDocument":{"completion":{"completionItem":{"snippetSupport":false}}}})JSON");
    request("initialize",
            Json::record({{"processId", Json{}},
                          {"rootUri", Json::string(fileUri(project.directory))},
                          {"capabilities", std::move(capabilities)}}),
            {"initialize", {}, "", {}});
}
void LspClient::stop()
{
    process_.cancel();
    if (started_)
        process_.wait();
    started_ = false;
    initialized_ = false;
    next_ = 1;
    pending_.clear();
    waiting_.clear();
    versions_.clear();
    encoding_ = "utf-16";
    framer_ = LspFramer{};
    std::lock_guard<std::mutex> lock(mutex_);
    incoming_.clear();
    events_.clear();
}
void LspClient::query(const std::string &method, const fs::path &file, const std::string &text,
                      size_t cursor)
{
    if (!running())
        throw std::runtime_error("clangd не запущен");
    auto uri = fileUri(file);
    int &version = versions_[uri];
    if (!version)
    {
        version = 1;
        notify("textDocument/didOpen",
               Json::record({{"textDocument", Json::record({{"uri", Json::string(uri)},
                                                            {"languageId", Json::string("cpp")},
                                                            {"version", Json::number("1")},
                                                            {"text", Json::string(text)}})}}));
    }
    else
    {
        ++version;
        notify("textDocument/didChange",
               Json::record({{"textDocument",
                              Json::record({{"uri", Json::string(uri)},
                                            {"version", Json::number(std::to_string(version))}})},
                             {"contentChanges",
                              Json::list({Json::record({{"text", Json::string(text)}})})}}));
    }
    Json params = Json::record(
        {{"textDocument", idObject(uri)}, {"position", textPosition(text, cursor, encoding_)}});
    if (method == "references")
        put(params, "context", Json::record({{"includeDeclaration", Json::flag(true)}}));
    request("textDocument/" + method, std::move(params), {method, file, text, {}});
}
std::vector<LspEvent> LspClient::poll()
{
    std::deque<Json> incoming;
    std::vector<LspEvent> out;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        incoming.swap(incoming_);
        while (!events_.empty())
        {
            out.push_back(std::move(events_.front()));
            events_.pop_front();
        }
    }
    for (auto &j : incoming)
    {
        auto method = str(j, "method");
        if (!method.empty())
        {
            if (method == "textDocument/publishDiagnostics")
            {
                auto params = get(j, "params");
                out.push_back(
                    {"diagnostics", uriFile(str(params, "uri")), "", get(params, "diagnostics")});
            }
            else if (get(j, "id").kind != Json::Kind::Null)
                send(Json::record({{"id", get(j, "id")}, {"result", Json{}}}));
            continue;
        }
        int id = int(integer(j, "id", -1));
        auto it = pending_.find(id);
        if (it == pending_.end())
            continue;
        auto event = std::move(it->second);
        pending_.erase(it);
        event.error = get(j, "error").kind != Json::Kind::Null;
        event.result = get(j, event.error ? "error" : "result");
        if (event.tag == "initialize")
        {
            if (event.error)
            {
                out.push_back(std::move(event));
                continue;
            }
            encoding_ = str(get(event.result, "capabilities"), "positionEncoding", "utf-16");
            initialized_ = true;
            notify("initialized", Json::record());
            for (auto &message : waiting_)
            {
                auto found = pending_.find(int(integer(message, "id", -1)));
                if (found != pending_.end())
                {
                    auto params = get(message, "params");
                    put(params, "position",
                        textPosition(found->second.text,
                                     textOffset(found->second.text, get(params, "position")),
                                     encoding_));
                    put(message, "params", std::move(params));
                }
                send(std::move(message));
            }
            waiting_.clear();
        }
        else
            out.push_back(std::move(event));
    }
    return out;
}
} // namespace tvstudio
