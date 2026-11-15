#include <atomic>
#include <condition_variable>
#include <regex>
#include <stdexcept>
#include <thread>
#include <tvstudio/process.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tvstudio
{
std::vector<std::string> commandLineArguments(int argc, char **argv)
{
    std::vector<std::string> out;
#ifdef _WIN32
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args)
        throw std::runtime_error("Не удалось прочитать командную строку");
    for (int i = 0; i < count; ++i)
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, args[i], -1, nullptr, 0, nullptr, nullptr);
        std::string value(size_t(n), 0);
        WideCharToMultiByte(CP_UTF8, 0, args[i], -1, value.data(), n, nullptr, nullptr);
        if (!value.empty())
            value.pop_back();
        out.push_back(std::move(value));
    }
    LocalFree(args);
    (void)argc;
    (void)argv;
#else
    for (int i = 0; i < argc; ++i)
        out.emplace_back(argv[i]);
#endif
    return out;
}
std::vector<std::string> splitArguments(const std::string &s)
{
    std::vector<std::string> out;
    std::string word;
    char quote = 0;
    bool started = false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        // Backslashes are path separators unless directly before a double quote.
        // There, use Windows argument rules: pairs are literal slashes and an
        // odd final slash escapes the quote. Single-quoted text is literal.
        if (c == '\\' && quote != '\'')
        {
            size_t end = i;
            while (end < s.size() && s[end] == '\\')
                ++end;
            size_t count = end - i;
            if (end < s.size() && s[end] == '"')
            {
                word.append(count / 2, '\\');
                if (count % 2)
                    word += '"';
                else
                    quote = quote ? 0 : '"';
                i = end;
            }
            else
            {
                word.append(count, '\\');
                i = end - 1;
            }
            started = true;
        }
        else if (quote)
        {
            if (c == quote)
                quote = 0;
            else
                word += c;
        }
        else if (c == '\'' || c == '"')
        {
            quote = c;
            started = true;
        }
        else if (c == ' ' || c == '\t' || c == '\n')
        {
            if (started)
            {
                out.push_back(word);
                word.clear();
                started = false;
            }
        }
        else
        {
            word += c;
            started = true;
        }
    }
    if (quote)
        throw std::runtime_error("Незакрытая кавычка в аргументах");
    if (started)
        out.push_back(word);
    return out;
}
std::string displayCommand(const std::vector<std::string> &args)
{
    std::string out;
    for (auto &a : args)
    {
        if (!out.empty())
            out += ' ';
        out += a.find_first_of(" \t\"") == std::string::npos ? a : quotedCpp(a);
    }
    return out;
}
struct Process::Impl
{
    std::atomic<bool> finished{false};
    std::atomic<int> result{-1};
    std::mutex mutex;
    std::condition_variable available;
    std::deque<std::string> pending;
    size_t pendingBytes = 0;
    bool shutdown = false;
    std::thread reader, errorReader, writer, waiter;
#ifdef _WIN32
    HANDLE process = nullptr, job = nullptr, input = nullptr, output = nullptr, error = nullptr;
    static void close(HANDLE &h)
    {
        if (h)
        {
            CloseHandle(h);
            h = nullptr;
        }
    }
    void kill()
    {
        if (job)
            TerminateJobObject(job, 130);
        else if (process)
            TerminateProcess(process, 130);
    }
    void read(HANDLE h, const Output &callback)
    {
        char data[8192];
        DWORD n;
        while (ReadFile(h, data, sizeof data, &n, nullptr) && n)
        {
            if (callback)
                try
                {
                    callback(std::string(data, n));
                }
                catch (...)
                {
                }
        }
    }
    void send(const std::string &s)
    {
        size_t offset = 0;
        while (offset < s.size())
        {
            DWORD n = 0;
            if (!WriteFile(input, s.data() + offset,
                           (DWORD)std::min<size_t>(s.size() - offset, 65536), &n, nullptr) ||
                !n)
                break;
            offset += n;
        }
    }
#else
    pid_t process = -1;
    int input = -1, output = -1, error = -1;
    static void close(int &fd)
    {
        if (fd >= 0)
        {
            ::close(fd);
            fd = -1;
        }
    }
    void kill()
    {
        if (process > 0)
        {
            ::kill(-process, SIGKILL);
            ::kill(process, SIGKILL);
        }
    }
    void read(int fd, const Output &callback)
    {
        char data[8192];
        while (true)
        {
            ssize_t n = ::read(fd, data, sizeof data);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            if (callback)
                try
                {
                    callback(std::string(data, size_t(n)));
                }
                catch (...)
                {
                }
        }
    }
    void send(const std::string &s)
    {
        size_t offset = 0;
        while (offset < s.size())
        {
            ssize_t n = ::write(input, s.data() + offset, s.size() - offset);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            offset += size_t(n);
        }
    }
#endif
    void writing()
    {
#ifndef _WIN32
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &mask, nullptr);
#endif
        while (true)
        {
            std::unique_lock<std::mutex> lock(mutex);
            available.wait(lock, [&] { return shutdown || !pending.empty(); });
            if (shutdown)
                break;
            auto data = std::move(pending.front());
            pending.pop_front();
            pendingBytes -= data.size();
            lock.unlock();
            send(data);
        }
    }
    void joinStreams()
    {
        if (reader.joinable())
            reader.join();
        if (errorReader.joinable())
            errorReader.join();
        {
            std::lock_guard<std::mutex> lock(mutex);
            shutdown = true;
        }
        available.notify_all();
        if (writer.joinable())
            writer.join();
    }
    ~Impl()
    {
        if (!finished)
            kill();
        {
            std::lock_guard<std::mutex> lock(mutex);
            shutdown = true;
        }
        available.notify_all();
        if (waiter.joinable())
            waiter.join();
        else
            joinStreams();
        close(input);
        close(output);
        close(error);
#ifdef _WIN32
        close(process);
        close(job);
#endif
    }
};
Process::Process() = default;
Process::~Process() = default;
namespace
{
#ifdef _WIN32
std::wstring wide(const std::string &s)
{
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
    if (!n)
        throw std::runtime_error("Недопустимая строка UTF-8 команды");
    std::wstring out(size_t(n), L' ');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}
std::wstring winQuote(const std::string &s)
{
    auto text = wide(s);
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : text)
    {
        if (c == L'\\')
        {
            ++slashes;
            continue;
        }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        out += c;
        slashes = 0;
    }
    out.append(slashes * 2, L'\\');
    return out + L'"';
}
void pipe(HANDLE &read, HANDLE &write, bool childReads)
{
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    if (!CreatePipe(&read, &write, &sa, 0))
        throw std::runtime_error("Не удалось создать канал процесса");
    SetHandleInformation(childReads ? write : read, HANDLE_FLAG_INHERIT, 0);
}
#else
void pipe(int (&fds)[2])
{
    if (::pipe(fds) != 0)
        throw std::runtime_error("Не удалось создать канал процесса");
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
}
#endif
} // namespace
void Process::start(const std::vector<std::string> &arguments, const fs::path &directory,
                    Output out, Output err, bool interactive)
{
    if (arguments.empty() || arguments[0].empty())
        throw std::runtime_error("Не задана команда процесса");
    impl_.reset();
    auto p = std::make_unique<Impl>();
#ifdef _WIN32
    HANDLE childInput = nullptr, childOutput = nullptr, childError = nullptr;
    try
    {
        if (!interactive)
        {
            pipe(childInput, p->input, true);
            pipe(p->output, childOutput, false);
            if (err)
                pipe(p->error, childError, false);
            else
                childError = childOutput;
        }
        std::wstring command;
        for (auto &a : arguments)
        {
            if (!command.empty())
                command += L' ';
            command += winQuote(a);
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof startup;
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = interactive ? GetStdHandle(STD_INPUT_HANDLE) : childInput;
        startup.hStdOutput = interactive ? GetStdHandle(STD_OUTPUT_HANDLE) : childOutput;
        startup.hStdError = interactive ? GetStdHandle(STD_ERROR_HANDLE) : childError;
        PROCESS_INFORMATION info{};
        p->job = CreateJobObjectW(nullptr, nullptr);
        if (!p->job)
            throw std::runtime_error("Не удалось создать группу процесса");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(p->job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
        auto cwd = fs::absolute(directory).wstring();
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                            CREATE_SUSPENDED | (interactive ? 0 : CREATE_NO_WINDOW), nullptr,
                            cwd.c_str(), &startup, &info))
            throw std::runtime_error("Не удалось запустить " + arguments[0] + ", код " +
                                     std::to_string(GetLastError()));
        p->process = info.hProcess;
        if (!AssignProcessToJobObject(p->job, p->process))
        {
            TerminateProcess(p->process, 1);
            CloseHandle(info.hThread);
            throw std::runtime_error("Не удалось назначить группу дочернему процессу");
        }
        ResumeThread(info.hThread);
        CloseHandle(info.hThread);
        if (childError == childOutput)
            childError = nullptr;
        Impl::close(childInput);
        Impl::close(childOutput);
        Impl::close(childError);
    }
    catch (...)
    {
        if (childError == childOutput)
            childError = nullptr;
        Impl::close(childInput);
        Impl::close(childOutput);
        Impl::close(childError);
        throw;
    }
#else
    int in[2] = {-1, -1}, output[2] = {-1, -1}, error[2] = {-1, -1};
    std::vector<char *> argv;
    for (auto &a : arguments)
        argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);
    auto cwd = utf8(fs::absolute(directory));
    try
    {
        if (!interactive)
        {
            pipe(in);
            pipe(output);
            if (err)
                pipe(error);
        }
        p->process = fork();
        if (p->process < 0)
            throw std::runtime_error("Не удалось создать процесс");
        if (p->process == 0)
        {
            setpgid(0, 0);
            if (chdir(cwd.c_str()) != 0)
                _exit(126);
            if (!interactive)
            {
                dup2(in[0], STDIN_FILENO);
                dup2(output[1], STDOUT_FILENO);
                dup2(err ? error[1] : output[1], STDERR_FILENO);
                for (int fd : {in[0], in[1], output[0], output[1], error[0], error[1]})
                    if (fd >= 0)
                        ::close(fd);
            }
            execvp(argv[0], argv.data());
            _exit(127);
        }
        setpgid(p->process, p->process);
        if (!interactive)
        {
            p->input = in[1];
            in[1] = -1;
            p->output = output[0];
            output[0] = -1;
            if (err)
            {
                p->error = error[0];
                error[0] = -1;
            }
        }
        for (auto f : {&in[0], &in[1], &output[0], &output[1], &error[0], &error[1]})
            Impl::close(*f);
    }
    catch (...)
    {
        for (auto f : {&in[0], &in[1], &output[0], &output[1], &error[0], &error[1]})
            Impl::close(*f);
        throw;
    }
#endif
    Impl *raw = p.get();
    if (!interactive)
    {
        p->reader = std::thread([raw, out] { raw->read(raw->output, out); });
        if (err)
            p->errorReader = std::thread([raw, err] { raw->read(raw->error, err); });
        p->writer = std::thread([raw] { raw->writing(); });
    }
    p->waiter = std::thread(
        [raw]
        {
            int result = 1;
#ifdef _WIN32
            WaitForSingleObject(raw->process, INFINITE);
            DWORD code = 1;
            GetExitCodeProcess(raw->process, &code);
            result = int(code);
            if (raw->job)
                TerminateJobObject(raw->job, code);
#else
            int status = 0;
            pid_t n;
            do
            {
                n = waitpid(raw->process, &status, 0);
            } while (n < 0 && errno == EINTR);
            if (n > 0)
                result = WIFEXITED(status) ? WEXITSTATUS(status)
                                           : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
            ::kill(-raw->process, SIGKILL);
#endif
            raw->joinStreams();
            raw->result = result;
            raw->finished = true;
        });
    impl_ = std::move(p);
}
void Process::write(std::string data)
{
    if (!impl_ || impl_->finished)
        throw std::runtime_error("Процесс не запущен");
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->shutdown)
        throw std::runtime_error("Процесс завершён");
    if (impl_->pendingBytes + data.size() > 16 * 1024 * 1024)
        throw std::runtime_error("Переполнена очередь ввода процесса");
    impl_->pendingBytes += data.size();
    impl_->pending.push_back(std::move(data));
    impl_->available.notify_one();
}
void Process::cancel()
{
    if (impl_ && !impl_->finished)
        impl_->kill();
}
bool Process::done() const
{
    return !impl_ || impl_->finished;
}
int Process::exitCode() const
{
    return impl_ ? impl_->result.load() : -1;
}
int Process::wait()
{
    if (!impl_)
        return -1;
    if (impl_->waiter.joinable())
        impl_->waiter.join();
    return impl_->result;
}
bool parseDiagnostic(const std::string &line, const fs::path &dir, Diagnostic &d)
{
    static const std::regex gcc(
        R"(^(.+?):([0-9]+)(?::([0-9]+))?:\s*(?:fatal )?(error|warning|note):\s*(.*)$)"),
        msvc(
            R"(^\s*(.+?)\(([0-9]+)(?:,([0-9]+))?\)\s*:\s*(?:fatal )?(error|warning|note)\s*[^:]*:\s*(.*)$)");
    std::smatch m;
    if (!std::regex_match(line, m, gcc) && !std::regex_match(line, m, msvc))
        return false;
    try
    {
        d.file = pathFromUtf8(trim(m[1]));
        if (d.file.is_relative())
            d.file = dir / d.file;
        d.line = std::max(1, std::stoi(m[2]));
        d.column = m[3].matched ? std::max(1, std::stoi(m[3])) : 1;
        d.endLine = d.line;
        d.endColumn = d.column + 1;
        d.severity = m[4] == "warning" ? 2 : m[4] == "note" ? 3 : 1;
        d.message = m[5];
        d.source = "Сборка";
        return true;
    }
    catch (const std::exception &)
    {
        return false;
    }
}
std::vector<std::vector<std::string>> buildCommands(const Project &p, const std::string &action)
{
    std::vector<std::vector<std::string>> result;
    auto dir = utf8(p.directory), build = utf8(p.buildDirectory());
    std::vector<std::string> configure = {p.build.cmake,
                                          "-S",
                                          dir,
                                          "-B",
                                          build,
                                          "-DCMAKE_BUILD_TYPE=" + p.build.profile};
    if (!p.build.generator.empty())
    {
        configure.push_back("-G");
        configure.push_back(p.build.generator);
    }
    if (!p.build.compiler.empty())
        configure.push_back("-DCMAKE_CXX_COMPILER=" + p.build.compiler);
    auto extra = splitArguments(p.build.extra);
    configure.insert(configure.end(), extra.begin(), extra.end());
    // База должна соответствовать тем же целям и параметрам, что и обычная сборка.
    configure.push_back("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON");
    if (action != "clean")
        result.push_back(std::move(configure));
    if (action == "configure")
        return result;
    std::vector<std::string> compile = {p.build.cmake, "--build", build, "--config",
                                        p.build.profile};
    if (action == "clean")
    {
        compile.push_back("--target");
        compile.push_back("clean");
    }
    else if (!p.build.target.empty())
    {
        compile.push_back("--target");
        compile.push_back(p.build.target);
    }
    if (p.build.jobs > 0)
    {
        compile.push_back("--parallel");
        compile.push_back(std::to_string(p.build.jobs));
    }
    if (action == "test")
        result.push_back(compile), result.push_back({"ctest", "--test-dir", build, "-C",
                                                     p.build.profile, "--output-on-failure"});
    else
        result.push_back(std::move(compile));
    return result;
}
BuildRunner::~BuildRunner()
{
    stop();
    process_.wait();
}
void BuildRunner::start(const Project &p, const std::string &action)
{
    if (active_)
        throw std::runtime_error("Сборка уже выполняется");
    commands_ = buildCommands(p, action);
    directory_ = p.directory;
    next_ = 0;
    cancelled_ = false;
    result_ = 0;
    active_ = true;
    launch();
}
void BuildRunner::launch()
{
    if (next_ >= commands_.size())
    {
        active_ = false;
        return;
    }
    auto command = commands_[next_++];
    {
        std::lock_guard<std::mutex> lock(mutex_);
        output_.push_back("$ " + displayCommand(command) + "\n");
    }
    try
    {
        process_.start(command, directory_,
                       [this](const std::string &text)
                       {
                           std::lock_guard<std::mutex> lock(mutex_);
                           if (output_.size() < 10000)
                               output_.push_back(text);
                       });
    }
    catch (const std::exception &e)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        output_.push_back(std::string(e.what()) + '\n');
        result_ = -1;
        active_ = false;
    }
}
void BuildRunner::stop()
{
    cancelled_ = true;
    process_.cancel();
}
void BuildRunner::poll()
{
    if (!active_ || !process_.done())
        return;
    result_ = process_.exitCode();
    if (cancelled_ || result_ != 0)
        active_ = false;
    else
        launch();
}
std::vector<std::string> BuildRunner::takeOutput()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out(output_.begin(), output_.end());
    output_.clear();
    return out;
}
} // namespace tvstudio
