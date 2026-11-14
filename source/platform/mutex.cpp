#include <internal/mutex.h>

namespace tvision
{

void SignalSafeMutexState::acquire(SignalSafeMutexState *self) noexcept
{
    ThreadId none {};
    ThreadId current {currentThreadId()};
    // Используем спин-блокировку: обычные мьютексы небезопасны в обработчиках сигналов.
    if (self)
        while (self->lockingThread.compare_exchange_weak(none, current))
            ;
}

} // пространство имён tvision
