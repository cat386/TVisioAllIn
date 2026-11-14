#ifndef TVISION_RECORDIO_H
#define TVISION_RECORDIO_H
#include <stddef.h>
#include <string.h>
#include <errno.h>

namespace tvision
{
// A stream may split a fixed-size record at any byte boundary.
// The callback must return -1 and set errno when no bytes are available.
template <size_t Size>
class RecordReader
{
    unsigned char bytes[Size];
    size_t used {0};
public:
    template <class Receive>
    bool read(void *record, Receive receive) noexcept
    {
        while (used < Size)
        {
            auto n = receive(bytes + used, Size - used);
            if (n > 0)
                used += (size_t) n;
            else if (n < 0 && errno == EINTR)
                continue;
            else
            {
                if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                    used = 0;
                return false;
            }
        }
        memcpy(record, bytes, Size);
        used = 0;
        return true;
    }
};
}
#endif
