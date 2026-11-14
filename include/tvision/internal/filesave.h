#ifndef TVISION_FILESAVE_H
#define TVISION_FILESAVE_H
#include <stddef.h>
namespace tvision
{
// Перед заменой целевого файла записывает и закрывает временный файл в том же каталоге. Нулевой путь
// резервной копии отключает её создание. При возврате False редактор сохраняет признак изменений.
bool saveFileAtomic(const char *path, const char *backup,
                    const char *first, size_t firstSize,
                    const char *second, size_t secondSize) noexcept;
}
#endif
