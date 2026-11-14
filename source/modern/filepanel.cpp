#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <tvision/filepanel.h>
#include <tvision/util.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace tvision
{
namespace
{
std::string joinPath(const std::string &dir, const std::string &name)
{
    return dir + (dir.empty() || dir.back() == '/' || dir.back() == '\\' ? "" : "/") + name;
}
std::string parentPath(std::string path)
{
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    size_t i = path.find_last_of("/\\");
    if (i == std::string::npos)
        return path;
    if (i == 0)
        return path.substr(0, 1);
    if (i == 2 && path.size() > 2 && path[1] == ':')
        return path.substr(0, 3);
    return path.substr(0, i);
}
std::string lowerAscii(std::string s)
{
    for (char &c : s)
        c = char(std::tolower((unsigned char)c));
    return s;
}
#ifdef _WIN32
std::wstring filePanelWide(const std::string &s)
{
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), -1, nullptr, 0);
    if (!n)
        throw std::runtime_error("Invalid UTF-8 path");
    std::vector<wchar_t> v((size_t)n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, v.data(), n);
    return v.data();
}
std::string filePanelUtf8(const wchar_t *s)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if (!n)
        throw std::runtime_error("Invalid path");
    std::vector<char> v((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, s, -1, v.data(), n, nullptr, nullptr);
    return v.data();
}
#endif
std::vector<PanelEntry> scanDirectory(const std::string &input, std::string &path)
{
    std::vector<PanelEntry> entries;
#ifdef _WIN32
    auto name = filePanelWide(input);
    DWORD needed = GetFullPathNameW(name.c_str(), 0, nullptr, nullptr);
    if (!needed)
        throw std::runtime_error("Cannot resolve directory");
    std::vector<wchar_t> absolute((size_t)needed);
    if (!GetFullPathNameW(name.c_str(), needed, absolute.data(), nullptr))
        throw std::runtime_error("Cannot resolve directory");
    DWORD attrs = GetFileAttributesW(absolute.data());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        throw std::runtime_error("Directory unavailable");
    path = filePanelUtf8(absolute.data());
    WIN32_FIND_DATAW data;
    auto pattern = filePanelWide(joinPath(path, "*"));
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() != ERROR_FILE_NOT_FOUND)
            throw std::runtime_error("Cannot list directory");
    }
    else
    {
        DWORD error = ERROR_NO_MORE_FILES;
        try
        {
            do
            {
                std::string n = filePanelUtf8(data.cFileName);
                if (n == "." || n == "..")
                    continue;
                PanelEntry e;
                e.name = n;
                e.path = joinPath(path, n);
                e.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                e.hidden = (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0 || n[0] == '.';
                e.symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
                e.size = (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
                uint64_t ticks = (uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32) |
                                 data.ftLastWriteTime.dwLowDateTime;
                e.modified = ticks >= 116444736000000000ULL
                                 ? int64_t((ticks - 116444736000000000ULL) / 10000000)
                                 : 0;
                entries.push_back(std::move(e));
            } while (FindNextFileW(find, &data));
            error = GetLastError();
        }
        catch (...)
        {
            FindClose(find);
            throw;
        }
        FindClose(find);
        if (error != ERROR_NO_MORE_FILES)
            throw std::runtime_error("Directory listing interrupted");
    }
#else
    char *resolved = realpath(input.c_str(), nullptr);
    if (!resolved)
        throw std::runtime_error(std::strerror(errno));
    path = resolved;
    free(resolved);
    DIR *dir = opendir(path.c_str());
    if (!dir)
        throw std::runtime_error(std::strerror(errno));
    try
    {
        errno = 0;
        while (auto *de = readdir(dir))
        {
            std::string n = de->d_name;
            if (n == "." || n == "..")
                continue;
            PanelEntry e;
            e.name = n;
            e.path = joinPath(path, n);
            e.hidden = n[0] == '.';
            struct stat st;
            if (lstat(e.path.c_str(), &st) != 0)
                throw std::runtime_error(std::strerror(errno));
            e.symlink = S_ISLNK(st.st_mode);
            if (e.symlink)
            {
                struct stat target;
                if (stat(e.path.c_str(), &target) == 0)
                    e.directory = S_ISDIR(target.st_mode);
            }
            else
                e.directory = S_ISDIR(st.st_mode);
            e.size = uint64_t(std::max<off_t>(0, st.st_size));
            e.modified = st.st_mtime;
            entries.push_back(std::move(e));
            errno = 0;
        }
        if (errno)
            throw std::runtime_error(std::strerror(errno));
    }
    catch (...)
    {
        closedir(dir);
        throw;
    }
    closedir(dir);
#endif
    std::string parent = parentPath(path);
    if (parent != path)
    {
        PanelEntry e;
        e.name = "..";
        e.path = parent;
        e.directory = true;
        e.parent = true;
        entries.insert(entries.begin(), std::move(e));
    }
    return entries;
}
} // namespace
FileClass PanelEntry::fileClass() const
{
    if (directory)
        return FileClass::Directory;
    size_t dot = name.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : lowerAscii(name.substr(dot + 1));
    const char *source[] = {"c", "cpp", "h", "hpp", "rs", "py", "js", "ts", "go", "java", "cs"};
    for (auto e : source)
        if (ext == e)
            return FileClass::Source;
    if (ext == "exe" || ext == "com" || ext == "bat" || ext == "cmd" || ext == "sh")
        return FileClass::Executable;
    if (ext == "zip" || ext == "gz" || ext == "xz" || ext == "7z" || ext == "tar" || ext == "rar")
        return FileClass::Archive;
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "svg" ||
        ext == "webp")
        return FileClass::Image;
    if (ext == "mp3" || ext == "wav" || ext == "mp4" || ext == "mkv" || ext == "ogg")
        return FileClass::Media;
    if (ext == "txt" || ext == "md" || ext == "pdf" || ext == "csv" || ext == "json" ||
        ext == "xml")
        return FileClass::Document;
    return FileClass::Other;
}
bool globMatch(const std::string &pattern, const std::string &text, bool sensitive)
{
    auto atoms = [](const std::string &s)
    {
        std::vector<std::string> out;
        size_t i = 0;
        while (i < s.size())
        {
            size_t n = TText::next(TStringView(s.data() + i, s.size() - i));
            if (!n)
                n = 1;
            out.push_back(s.substr(i, n));
            i += n;
        }
        return out;
    };
    auto p = atoms(pattern), s = atoms(text);
    size_t i = 0, j = 0, star = std::string::npos, retry = 0;
    auto equal = [&](const std::string &a, const std::string &b)
    {
        if (sensitive)
            return a == b;
        if (a.size() == 1 && b.size() == 1)
            return std::tolower((unsigned char)a[0]) == std::tolower((unsigned char)b[0]);
        return bool(TText::equalsIgnoreCase(a.c_str(), b.c_str()));
    };
    while (j < s.size())
    {
        if (i < p.size() && (p[i] == "?" || equal(p[i], s[j])))
        {
            ++i;
            ++j;
        }
        else if (i < p.size() && p[i] == "*")
        {
            star = i++;
            retry = j;
        }
        else if (star != std::string::npos)
        {
            i = star + 1;
            j = ++retry;
        }
        else
            return false;
    }
    while (i < p.size() && p[i] == "*")
        ++i;
    return i == p.size();
}
std::string formatFileSize(uint64_t size)
{
    if (size < 1024)
        return std::to_string(size) + " B";
    const char *units[] = {"KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    long double value = size;
    size_t i = 0;
    value /= 1024;
    while (value >= 1024 && i < 5)
    {
        value /= 1024;
        ++i;
    }
    char out[64];
    std::snprintf(out, sizeof(out), "%.1Lf %s", value, units[i]);
    return out;
}
FilePanelModel::FilePanelModel(const std::string &p)
{
    if (!changeDirectory(p))
        throw std::runtime_error(error_);
}
bool FilePanelModel::changeDirectory(const std::string &p)
{
    try
    {
        std::string path;
        auto entries = scanDirectory(p, path);
        entries_ = std::move(entries);
        path_ = std::move(path);
        rebuild();
        error_.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error_ = e.what();
        return false;
    }
}
bool FilePanelModel::refresh()
{
    std::set<std::string> marked;
    for (const auto &e : entries_)
        if (e.marked)
            marked.insert(e.path);
    if (!changeDirectory(path_))
        return false;
    for (auto &e : entries_)
        e.marked = marked.count(e.path) != 0;
    return true;
}
void FilePanelModel::rebuild()
{
    visible_.clear();
    for (size_t i = 0; i < entries_.size(); ++i)
    {
        const auto &e = entries_[i];
        if (e.parent || ((hidden_ || !e.hidden) &&
                         (e.directory || filter_.empty() || globMatch(filter_, e.name))))
            visible_.push_back(i);
    }
    std::stable_sort(visible_.begin(), visible_.end(),
                     [&](size_t x, size_t y)
                     {
                         const auto &a = entries_[x], &b = entries_[y];
                         if (a.parent != b.parent)
                             return a.parent;
                         if (a.directory != b.directory)
                             return a.directory;
                         int c = 0;
                         if (sort_ == 1)
                             c = a.size == b.size ? 0 : a.size < b.size ? -1 : 1;
                         else if (sort_ == 3)
                             c = a.modified == b.modified ? 0 : a.modified < b.modified ? -1 : 1;
                         else if (sort_ == 2)
                             c = int(a.fileClass()) - int(b.fileClass());
                         if (!c)
                             c = naturalCompare(a.name, b.name);
                         return reverse_ ? c > 0 : c < 0;
                     });
}
void FilePanelModel::setFilter(std::string p)
{
    filter_ = std::move(p);
    rebuild();
}
void FilePanelModel::showHidden(bool value)
{
    hidden_ = value;
    rebuild();
}
const PanelEntry *FilePanelModel::entry(size_t r) const
{
    return r < visible_.size() ? &entries_[visible_[r]] : nullptr;
}
std::string FilePanelModel::header(size_t c) const
{
    static const char *names[] = {"Name", "Size", "Type", "Modified"};
    return c < 4 ? names[c] : "";
}
std::string FilePanelModel::cell(size_t r, size_t c) const
{
    auto *e = entry(r);
    if (!e)
        return "";
    switch (c)
    {
    case 0:
        return std::string(e->marked ? "* " : "  ") + e->name +
               (e->directory && !e->parent ? "/" : "");
    case 1:
        return e->directory ? "<DIR>" : formatFileSize(e->size);
    case 2:
    {
        static const char *names[] = {"directory", "executable", "source",   "archive",
                                      "image",     "media",      "document", "file"};
        return names[int(e->fileClass())];
    }
    case 3:
        return e->parent ? "" : std::to_string(e->modified);
    default:
        return "";
    }
}
bool FilePanelModel::sort(size_t c, bool asc)
{
    if (c >= 4)
        return false;
    sort_ = c;
    reverse_ = !asc;
    rebuild();
    return true;
}
void FilePanelModel::toggleMark(size_t r)
{
    if (r < visible_.size() && !entries_[visible_[r]].parent)
        entries_[visible_[r]].marked = !entries_[visible_[r]].marked;
}
void FilePanelModel::markGlob(const std::string &pattern, bool value)
{
    for (auto &e : entries_)
        if (!e.parent && globMatch(pattern, e.name))
            e.marked = value;
}
void FilePanelModel::invertMarks()
{
    for (auto &e : entries_)
        if (!e.parent)
            e.marked = !e.marked;
}
std::vector<std::string> FilePanelModel::markedPaths() const
{
    std::vector<std::string> out;
    for (const auto &e : entries_)
        if (e.marked)
            out.push_back(e.path);
    return out;
}
TColorAttr TFilePanel::rowColor(size_t row, bool selected, bool header)
{
    TColorAttr attr = TTable::rowColor(row, selected, header);
    if (!selected && !header && colorByType)
    {
        auto *e = files()->entry(row);
        if (e)
            attr.setForeground(TColorXTerm(typeColors[(size_t)e->fileClass()]));
    }
    return attr;
}
TFilePanel::TFilePanel(const TRect &r, const std::string &p)
    : TTable(r, std::make_shared<FilePanelModel>(p),
             {TableColumn("Name", 24), TableColumn("Size", 10, Align::Right),
              TableColumn("Type", 12), TableColumn("Modified", 12)})
{
    tabs_.push_back(std::static_pointer_cast<FilePanelModel>(model()));
    cursors_.push_back(0);
    frozenColumns = 1;
}
bool TFilePanel::changeDirectory(const std::string &p)
{
    if (!files()->changeDirectory(p))
        return false;
    setCursor(0, 0);
    notify(cmDirectoryChanged);
    return true;
}
bool TFilePanel::enter()
{
    auto *e = files()->entry(row());
    if (!e)
        return false;
    if (e->directory)
    {
        std::string path = e->path;
        return changeDirectory(path);
    }
    notify(cmFileOpen);
    return true;
}
bool TFilePanel::goUp()
{
    return changeDirectory(parentPath(files()->path()));
}
void TFilePanel::newTab(const std::string &p)
{
    auto next = std::make_shared<FilePanelModel>(p.empty() ? files()->path() : p);
    cursors_[tab_] = row();
    tabs_.push_back(next);
    cursors_.push_back(0);
    tab_ = tabs_.size() - 1;
    setModel(next);
}
bool TFilePanel::closeTab()
{
    if (tabs_.size() < 2)
        return false;
    tabs_.erase(tabs_.begin() + (ptrdiff_t)tab_);
    cursors_.erase(cursors_.begin() + (ptrdiff_t)tab_);
    tab_ = std::min(tab_, tabs_.size() - 1);
    setModel(tabs_[tab_]);
    setCursor(cursors_[tab_], 0);
    return true;
}
void TFilePanel::selectTab(size_t i)
{
    if (i >= tabs_.size())
        return;
    cursors_[tab_] = row();
    tab_ = i;
    setModel(tabs_[i]);
    setCursor(cursors_[i], 0);
}
void TFilePanel::handleEvent(TEvent &ev)
{
    if (ev.what == evKeyDown)
    {
        switch (ev.keyDown.keyCode)
        {
        case kbEnter:
            enter();
            clearEvent(ev);
            return;
        case kbBack:
            goUp();
            clearEvent(ev);
            return;
        case kbIns:
            files()->toggleMark(row());
            setCursor(row() + 1, column());
            clearEvent(ev);
            return;
        default:
            break;
        }
    }
    TTable::handleEvent(ev);
}
TDualPanel::TDualPanel(const TRect &r, const std::string &l, const std::string &rr) : TGroup(r)
{
    options |= ofSelectable;
    std::unique_ptr<TFilePanel> left(new TFilePanel(TRect(0, 0, size.x / 2, size.y), l));
    std::unique_ptr<TFilePanel> right(new TFilePanel(TRect(size.x / 2, 0, size.x, size.y), rr));
    left_ = left.release();
    right_ = right.release();
    insert(left_);
    insert(right_);
    selectNext(False);
}
TFilePanel &TDualPanel::active() const
{
    return current == right_ ? *right_ : *left_;
}
TFilePanel &TDualPanel::other() const
{
    return current == right_ ? *left_ : *right_;
}
void TDualPanel::switchPanel()
{
    setCurrent(&other(), normalSelect);
}
void TDualPanel::syncOther()
{
    other().changeDirectory(active().files()->path());
}
void TDualPanel::changeBounds(const TRect &r)
{
    TGroup::changeBounds(r);
    left_->changeBounds(TRect(0, 0, size.x / 2, size.y));
    right_->changeBounds(TRect(size.x / 2, 0, size.x, size.y));
}
} // namespace tvision
