/*
 * dos.h определяет структуры, объединения, макросы и функции для работы
 * с MS-DOS и семейством микропроцессоров Intel iAPX86.
 *
 *
 */
/*
 *      C/C++ Run Time Library - Version 6.0
 *
 *      Copyright (c) 1987, 1993 by Borland International
 *      All Rights Reserved.
 *
 */

#ifdef __BORLANDC__
#include <dos.h>
#else

#ifdef TVISION_COMPAT_DOS_INCNEXT
#undef TVISION_COMPAT_DOS_INCNEXT
#include_next <dos.h>
#endif // TVISION_COMPAT_DOS_INCNEXT

#ifndef TVISION_COMPAT_DOS_H
#define TVISION_COMPAT_DOS_H

#include "_defs.h"

#ifdef _MSC_VER
#include <corecrt.h>
#elif defined(__MINGW32__)
#define TVISION_COMPAT_DOS_INCNEXT
#include <dos.h>
#undef TVISION_COMPAT_DOS_INCNEXT
#endif

#include <errno.h>
#include <stdint.h>

#define FA_NORMAL   0x00        /* Обычный файл без атрибутов. */
#define FA_RDONLY   0x01        /* Атрибут «только чтение». */
#define FA_HIDDEN   0x02        /* Скрытый файл. */
#define FA_SYSTEM   0x04        /* Системный файл. */
#define FA_LABEL    0x08        /* Метка тома. */
#define FA_DIREC    0x10        /* Каталог. */
#define FA_ARCH     0x20        /* Архивный атрибут. */

/* Имена атрибутов файлов в MSC. */

#define _A_NORMAL   0x00        /* Обычный файл без атрибутов. */
#define _A_RDONLY   0x01        /* Атрибут «только чтение». */
#define _A_HIDDEN   0x02        /* Скрытый файл. */
#define _A_SYSTEM   0x04        /* Системный файл. */
#define _A_VOLID    0x08        /* Метка тома. */
#define _A_SUBDIR   0x10        /* Каталог. */
#define _A_ARCH     0x20        /* Архивный атрибут. */

#ifndef _FFBLK_DEF
#define _FFBLK_DEF
struct  ffblk   {
    int32_t         ff_reserved;
    int32_t         ff_fsize;
    uint32_t        ff_attrib;
    unsigned short  ff_ftime;
    unsigned short  ff_fdate;
    char            ff_name[256];
};
#endif  /* __FFBLK_DEF */

/* Структура MSC find_t в точности соответствует ffblk. */
struct  find_t {
    int32_t         reserved;
    int32_t         size;              /* Размер файла. */
    uint32_t        attrib;            /* Байт атрибутов найденного файла. */
    unsigned short  wr_time;           /* Время последней записи в файл. */
    unsigned short  wr_date;           /* Дата последней записи в файл. */
    char            name[256];         /* Имя найденного файла с завершающим нулём. */
};

#include <stdio.h> // SEEK_SET, SEEK_CUR, SEEK_END

unsigned _dos_findfirst( const char * __path, unsigned __attrib, struct find_t *__finfo ) noexcept;
unsigned _dos_findnext( struct find_t *__finfo ) noexcept;

#endif // TVISION_COMPAT_DOS_H

#endif // __BORLANDC__
