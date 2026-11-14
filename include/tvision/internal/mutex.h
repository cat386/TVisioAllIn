#ifndef TVISION_MUTEX_H
#define TVISION_MUTEX_H

#include <atomic>

namespace tvision
{

#if ATOMIC_POINTER_LOCK_FREE < 2
#warning The code below assumes that atomic pointers are lock-free, but they are not.
#endif

// Мьютекс на основе спин-блокировки, безопасный для обработчиков сигналов, в отличие от мьютексов STL.
struct SignalSafeMutexState
{
    using ThreadId = const void *;

    std::atomic<ThreadId> lockingThread {};

    static ThreadId currentThreadId() noexcept
    {
        static thread_local struct {} idBase;
        return &idBase;
    }

    bool lockedByCurrentThread() noexcept
    {
        return lockingThread == currentThreadId();
    }

    static void acquire(SignalSafeMutexState *self) noexcept;

    static void release(SignalSafeMutexState *self) noexcept
    {
        if (self)
            self->lockingThread = ThreadId {};
    }

    struct LockGuard
    {
        SignalSafeMutexState *self;

        LockGuard(SignalSafeMutexState *aSelf) noexcept :
            self(aSelf)
        {
            acquire(self);
        }

        ~LockGuard()
        {
            release(self);
        }
    };
};

template <class T>
struct SignalSafeReentrantMutex
{
    T elem;
    SignalSafeMutexState state;

    template <class Func>
    // func принимает параметр типа T &.
    auto lock(Func &&func) noexcept
    {
        SignalSafeMutexState::LockGuard lk {lockedByCurrentThread() ? nullptr
                                                                    : &state};
        return func(elem);
    }

    bool lockedByCurrentThread() noexcept
    {
        return state.lockedByCurrentThread();
    }
};

} // пространство имён tvision

#endif // TVISION_MUTEX_H
