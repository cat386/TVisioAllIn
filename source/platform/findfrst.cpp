#include <internal/findfrst.h>
#include <internal/pathconv.h>

#define SPECIAL_BITS (_A_SUBDIR|_A_HIDDEN|_A_SYSTEM)

namespace tvision
{

FindFirstRec::RecList FindFirstRec::recList;

FindFirstRec* FindFirstRec::allocate(struct find_t *fileinfo, unsigned attrib,
                                     const char *pathname) noexcept
{
    // Интерфейс findfirst, основанный на вызове DOS 0x4E, не имеет
    // findclose. Используется стратегия RTL Borland: выделяем новые потоки
    // каталогов, если адрес fileinfo ранее нам не передавался.
    //
    if (!fileinfo)
        return 0;
    std::lock_guard<std::mutex> lock(recList);
    FindFirstRec *r = 0;
    size_t index;

    for (index = 0; index < recList.size(); ++index)
        if (recList[index].finfo == fileinfo)
        {
            r = &recList[index];
            break;
        }
    // Теперь index — либо позиция соответствующего элемента FindFirstRec,
    // либо размер recList. Во втором случае элемент будет добавлен в конец,
    // и index станет его корректным индексом.
    if (r)
        r->close();
    else
    {
        recList.emplace_back();
        r = &recList[index];
        r->finfo = fileinfo;
    }
    // Если pathname — допустимый каталог, связываем fileinfo с выделенным
    // FindFirstRec. Иначе возвращаем NULL.
    if (r->setParameters(attrib, pathname))
    {
        fileinfo->reserved = (long) index;
        return r;
    }
    else
        return 0;
}

FindFirstRec* FindFirstRec::get(struct find_t *fileinfo) noexcept
{
    // Возвращаем экземпляр FindFirstRec, на который указывает fileinfo.
    std::lock_guard<std::mutex> lock(recList);
    size_t pos = fileinfo->reserved;
    if (pos < recList.size() && recList[pos].finfo == fileinfo)
        return &recList[pos];
    return 0;
}

#ifndef _WIN32

bool FindFirstRec::setParameters(unsigned attrib, const char *pathname) noexcept
{
    if (!dirStream)
    {
        searchAttr = attrib;
        if (setPath(pathname))
            return open();
    }
    return false;
}

bool FindFirstRec::next() noexcept
{
    struct dirent *e;
    bool found = false;
    while ((e = readdir(dirStream)) && !(found = matchEntry(e)));
    if (!e)
        close();
    return found;
}

bool FindFirstRec::open() noexcept
{
    if (!dirStream)
        return (dirStream = opendir(searchDir.c_str()));
    return false;
}

void FindFirstRec::close() noexcept
{
    if (dirStream)
        closedir(dirStream), dirStream = 0;
}

bool FindFirstRec::setPath(const char* pathname) noexcept
{
    // Отклоняем NULL и пустые пути.
    if (pathname && *pathname)
    {
        searchDir = pathname;
        path_dos2unix(searchDir);
        // Win32 FindFirst отклоняет пути с завершающим разделителем. Но старый
        // код, не учитывающий разделители Unix, может не удалить его и вызвать
        // findfirst с таким путём. Поэтому корректно обрабатываем этот случай.
        //
        if (searchDir.back() == '/')
            wildcard = '.';
        else
        {
            auto lastSlash = searchDir.find_last_of('/');
            // Если в pathname нет '/', wildcard содержит всю строку, а searchDir
            // задаётся текущим каталогом.
            wildcard = searchDir.substr(lastSlash + 1);
            if (lastSlash == std::string::npos)
                searchDir = "./";
            else
                searchDir = searchDir.substr(0, lastSlash + 1);
            // '*.*' означает любое имя и расширение, поэтому точка в имени не
            // обязательна. В 32-битной сборке Borland C++ '*' даёт тот же результат;
            // в 16-битной возвращает только файлы без расширения.
            //
            if (wildcard == "*.*")
                wildcard = '*';
        }
        // Теперь searchDir всегда заканчивается на '/'.
        return true;
    }
    return false;
}

bool FindFirstRec::matchEntry(struct dirent* e) noexcept
{
    struct stat st;
    if (wildcardMatch(wildcard.c_str(), e->d_name) &&
        stat((searchDir + e->d_name).c_str(), &st) == 0)
    {
        unsigned fileAttr = cvtAttr(&st, e->d_name);
        if (attrMatch(fileAttr))
        {
            // Совпадение найдено, заполняем finfo.
            finfo->size = st.st_size;
            finfo->attrib = fileAttr;
            cvtTime(&st, finfo);
            strnzcpy(finfo->name, e->d_name, sizeof(find_t::name));
            return true;
        }
    }
    return false;
}

bool FindFirstRec::attrMatch(unsigned attrib) noexcept
{
    // Поведение исходного _dos_findnext: если запрошены атрибуты скрытого,
    // системного файла или подкаталога, возвращаются обычные файлы и файлы с
    // любым из запрошенных атрибутов.
    return !(attrib & SPECIAL_BITS) || (searchAttr & attrib & SPECIAL_BITS);
}

bool FindFirstRec::wildcardMatch(char const *wildcard, char const *filename) noexcept
{
    // https://stackoverflow.com/a/3300547
    for (; *wildcard != '\0'; ++wildcard)
        switch (*wildcard)
        {
            case '?':
                if (*filename == '\0')
                    return false;
                ++filename;
                break;
            case '*':
                if (wildcard[1] == '\0')
                    return true;
                for (size_t i = 0; filename[i] != '\0'; ++i)
                    if (wildcardMatch(wildcard + 1, &filename[i]))
                        return true;
                return false;
            default:
                if (*filename != *wildcard)
                    return false;
                ++filename;
        }
    return *filename == '\0';
}

unsigned FindFirstRec::cvtAttr(const struct stat *st, const char* filename) noexcept
{
    // Возвращает атрибуты файла в формате find_t.
    unsigned attr = 0; // _A_NORMAL
    if (filename[0] == '.')
        attr |= _A_HIDDEN;
    if (st->st_mode & S_IFDIR)
        attr |= _A_SUBDIR;
    else if (!(st->st_mode & S_IFREG)) // Если это не обычный файл.
        attr |= _A_SYSTEM;
    else if (!(st->st_mode & S_IWUSR)) // Если нет права записи; проверка неточная.
        attr |= _A_RDONLY;
    return attr;
}

void FindFirstRec::cvtTime(const struct stat *st, struct find_t *fileinfo) noexcept
{
    // Обновляет времена fileinfo значениями из st.
    struct FatDate {
        ushort  day     : 5, // День месяца, 1–31.
                month   : 4, // Месяц, 1–12.
                year    : 7; // Год минус 1980.
    } *wr_date = (FatDate *) &fileinfo->wr_date;
    struct FatTime {
        ushort  sec     : 5, // Секунды, делённые на два.
                min     : 6, // Минуты, 0–59.
                hour    : 5; // Часы, 0–23.
    } *wr_time = (FatTime *) &fileinfo->wr_time;

    struct tm *lt = localtime(&st->st_mtime);
    *wr_date = { ushort (lt->tm_mday),
                 ushort (lt->tm_mon + 1),
                 ushort (lt->tm_year - 80) };
    *wr_time = { ushort (lt->tm_sec/2),
                 ushort (lt->tm_min),
                 ushort (lt->tm_hour) };
}

#else

bool FindFirstRec::setParameters(unsigned attrib, const char *pathname) noexcept
{
    if (hFindFile != INVALID_HANDLE_VALUE)
        close();
    fileName.assign(pathname);
    searchAttr = attrib;
    return open();
}

bool FindFirstRec::next() noexcept
{
    WIN32_FIND_DATAW findData;
    while (true)
    {
        if (hFindFile == INVALID_HANDLE_VALUE)
        {
            MultiByteToWideChar(CP_UTF8, 0,
                                fileName.c_str(), -1,
                                findData.cFileName, sizeof(findData.cFileName)/sizeof(wchar_t));
            hFindFile = FindFirstFileW(findData.cFileName, &findData);
        }
        else if (!FindNextFileW(hFindFile, &findData))
        {
            close();
            return false;
        }

        if (hFindFile != INVALID_HANDLE_VALUE)
        {
            unsigned attr = cvtAttr(&findData, findData.cFileName);
            if (attrMatch(attr)) {
                // Совпадение найдено, заполняем finfo.
                finfo->size = findData.nFileSizeLow;
                finfo->attrib = attr;
                cvtTime(&findData, finfo);
                WideCharToMultiByte(CP_UTF8, 0,
                                    findData.cFileName, -1,
                                    finfo->name, sizeof(finfo->name),
                                    nullptr, nullptr);
                return true;
            }
        }
        else
            return false;
    }
}

bool FindFirstRec::open() noexcept
{
    return true;
}

void FindFirstRec::close() noexcept
{
    if (hFindFile != INVALID_HANDLE_VALUE)
        FindClose(hFindFile), hFindFile = INVALID_HANDLE_VALUE;
}

bool FindFirstRec::attrMatch(unsigned attrib) noexcept
{
    return ((searchAttr & _A_VOLID) && (attrib & _A_VOLID))
        || !(attrib & SPECIAL_BITS)
        || (searchAttr & attrib & SPECIAL_BITS);
}

unsigned FindFirstRec::cvtAttr(const WIN32_FIND_DATAW *findData, const wchar_t* filename) noexcept
{
    unsigned attr = findData->dwFileAttributes;
    if (filename[0] == L'.')
        attr |= _A_HIDDEN;
    return attr;
}

void FindFirstRec::cvtTime(const WIN32_FIND_DATAW *findData, struct find_t *fileinfo) noexcept
{
    FILETIME localTime;
    FileTimeToLocalFileTime(&findData->ftLastWriteTime, &localTime);
    FileTimeToDosDateTime(&localTime, &fileinfo->wr_date, &fileinfo->wr_time);
}

#endif // _WIN32

} // пространство имён tvision
