// Замены функций библиотеки времени выполнения C/C++ Borland. Код ниже
// написан с нуля, хотя основан на идеях RTL 1.5 из BC++ 4.52.
//

#include <internal/findfrst.h>
#include <internal/pathconv.h>
#include <dir.h>

#ifdef _WIN32
#include <bitset>
#endif

unsigned _dos_findfirst( const char *pathname, unsigned attrib,
                         struct find_t *fileinfo ) noexcept
{
    using namespace tvision;
    // Исходный findfirst устанавливает errno при ошибке. Пока этого не делаем.
    FindFirstRec *r;
    if ((r = FindFirstRec::allocate(fileinfo, attrib, pathname)))
        return r->next() ? 0 : -1;
    return -1;
}

unsigned _dos_findnext(struct find_t *fileinfo) noexcept
{
    using namespace tvision;
    FindFirstRec *r;
    if ((r = FindFirstRec::get(fileinfo)) && r->next())
        return 0;
    return -1;
}

int findfirst(const char *pathname, struct ffblk *ffblk, int attrib) noexcept
{
    return _dos_findfirst(pathname, attrib, (struct find_t *) ffblk);
}

int findnext(struct ffblk *ffblk) noexcept
{
    return _dos_findnext((struct find_t *) ffblk);
}

static size_t movestr(char *dest, TStringView src, size_t size) noexcept
{
    if (size)
    {
        size_t copy_bytes = src.size();
        if (copy_bytes > size - 1)
            copy_bytes = size - 1;
        memmove(dest, src.data(), copy_bytes);
        dest[copy_bytes] = '\0';
        return copy_bytes;
    }
    return 0;
}

void fnmerge( char *pathP, const char *driveP, const char *dirP,
              const char *nameP, const char *extP ) noexcept
{
    using namespace tvision;
    (void) driveP;
    size_t n = 0;
#ifdef _WIN32
    if (driveP && *driveP)
    {
        n += movestr(&pathP[n], driveP, MAXPATH - n);
        if (pathP[n-1] != ':')
            n += movestr(&pathP[n], ":", MAXPATH - n);
    }
#endif
    if (dirP && *dirP)
    {
        n += movestr(&pathP[n], dirP, MAXPATH - n);
        if (pathP[n-1] != '\\' && pathP[n-1] != '/')
            n += movestr(&pathP[n], "\\", MAXPATH - n);
    }
    if (nameP && *nameP)
        n += movestr(&pathP[n], nameP, MAXPATH - n);
    if (extP && *extP)
    {
        if (*extP != '.')
            n += movestr(&pathP[n], ".", MAXPATH - n);
        n += movestr(&pathP[n], extP, MAXPATH - n);
    }
#ifdef _TV_UNIX
    // fnmerge часто вызывается перед доступом к файлам, поэтому путь в формате
    // Unix исправляет большинство случаев обращения к файловой системе.
    path_dos2unix(pathP);
#endif
}

static bool CopyComponent(char* dstP, const char* startP, const char* endP, size_t MAX) noexcept
{
    // Копирует компонент пути длиной endP-startP в dstP. Для пустого
    // компонента возвращает false и ничего не делает.
    if (endP != startP)
    {
        if (dstP)
            // Всегда добавляет завершающий '\0', поэтому нужен +1.
            strnzcpy(dstP, startP, min<size_t>(MAX, endP-startP+1));
        return true;
    }
    return false;
}

int fnsplit(const char *pathP, char *driveP, char *dirP, char *nameP, char *extP) noexcept
{
    int flags = 0;
    if (driveP)
        memset(driveP, 0, MAXDRIVE);
    if (dirP)
        memset(dirP, 0, MAXDIR);
    if (nameP)
        memset(nameP, 0, MAXFILE);
    if (extP)
        memset(extP, 0, MAXEXT);
    if (pathP && *pathP)
    {
        size_t len = strlen(pathP);
        const char *pathEnd = pathP+len;
        const char *caretP = 0;
        const char *slashP = 0; // Крайняя правая косая черта.
        const char *lastDotP = 0; // Последняя точка в имени файла.
        const char *firstDotP = 0; // Первая точка в имени файла.
        for (size_t i = len - 1; i < len; --i)
            switch (pathP[i])
            {
                case '?':
                case '*':
                    // Подстановочные символы распознаются только в имени или расширении.
                    if (!slashP)
                        flags |= WILDCARDS;
                    break;
                case '.':
                    if (!slashP)
                    {
                        if (!lastDotP)
                            lastDotP = pathP+i;
                        firstDotP = pathP+i;
                    }
                    break;
                case '\\':
                case '/':
                    if (!slashP)
                        slashP = pathP+i;
                    break;
                case ':':
                    if (i == 1)
                    {
                        caretP = pathP+i;
                        i = 0; // Выходим из цикла; букву диска не проверяем.
                    }
                default:
                    ;
            }
        // Переменные указывают за последний символ каждого компонента.
        const char *driveEnd = caretP ? caretP+1 : pathP;
        const char *dirEnd = slashP ? slashP+1 : driveEnd;
        const char *nameEnd = lastDotP ? lastDotP : pathEnd;
        // Особый случай: путь заканчивается на '.' или '..', поэтому имени файла нет.
        if (lastDotP == pathEnd-1 && lastDotP - firstDotP < 2 && firstDotP == dirEnd)
            dirEnd = nameEnd = pathEnd;
        // Копируем компоненты и задаём флаги.
        CopyComponent(driveP, pathP, driveEnd, MAXDRIVE) && (flags |= DRIVE);
        CopyComponent(dirP, driveEnd, dirEnd, MAXDIR) && (flags |= DIRECTORY);
        CopyComponent(nameP, dirEnd, nameEnd, MAXFILE) && (flags |= FILENAME);
        CopyComponent(extP, nameEnd, pathEnd, MAXEXT) && (flags |= EXTENSION);
    }
    return flags;
}

int getdisk() noexcept
{
#ifdef _WIN32
    return _getdrive() - 1;
#else
    // Эмулируем диск C.
    return 'C' - 'A';
#endif
}

int setdisk(int drive) noexcept
{
#ifdef _WIN32
    _chdrive(drive - 1);
    return std::bitset<8*sizeof(ulong)>(_getdrives()).count();
#else
    return drive == getdisk() ? 0 : -1;
#endif
}

int getcurdir(int drive, char *direc) noexcept
{
    // direc — массив длиной MAXDIR для имени каталога с завершающим нулём,
    // без диска и начальной обратной косой черты. Диск 0 означает диск по
    // умолчанию, 1 — диск A и далее.
    using namespace tvision;
#ifdef _WIN32
    enum { prefix = 3 }; // Диск и косая черта.
    enum { bufLen = MAXDIR + prefix };
    wchar_t buf[bufLen];
    wchar_t drv[3] = {wchar_t((drive ? drive - 1 : getdisk()) + 'A'), ':', '\0'};

    if ( GetFullPathNameW(drv, bufLen, buf, nullptr) <= bufLen
         && WideCharToMultiByte(CP_UTF8, 0, buf + prefix, -1, direc, MAXDIR, nullptr, nullptr) )
        return 0;
    return -1;
#else
    enum { prefix = 1 }; // Косая черта корня.
    if (drive == 0 || drive - 1 == getdisk())
    {
        char buf[MAXDIR + prefix];
        if (getcwd(buf, MAXDIR + prefix))
        {
            path_unix2dos(buf);
            strnzcpy(direc, buf + prefix, MAXDIR);
            return 0;
        }
    }
    return -1;
#endif
}
