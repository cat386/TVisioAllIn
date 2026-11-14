#ifndef TVISION_FILESAVE_H
#define TVISION_FILESAVE_H
#include <stddef.h>
namespace tvision
{
// Write and close a sibling temporary file before replacing the destination.
// A null backup path disables backups. False leaves the editor modified.
bool saveFileAtomic(const char *path, const char *backup,
                    const char *first, size_t firstSize,
                    const char *second, size_t secondSize) noexcept;
}
#endif
