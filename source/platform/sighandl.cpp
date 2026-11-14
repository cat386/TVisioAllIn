#include <internal/sighandl.h>

#ifdef _TV_UNIX

#include <stdlib.h>

namespace tvision
{

std::atomic<SignalHandlerCallback *> SignalHandler::callback {nullptr};
const int SignalHandler::handledSignals[HandledSignalCount] =
    { SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGBUS, SIGFPE, SIGSEGV, SIGPIPE, SIGTERM, SIGTSTP };

static bool operator==(const struct sigaction &a, const struct sigaction &b) noexcept
{
    constexpr int knownFlags =
        SA_NOCLDSTOP | SA_NOCLDWAIT | SA_SIGINFO | SA_ONSTACK | SA_RESTART |
        SA_NODEFER | SA_RESETHAND;
    return ((a.sa_flags & knownFlags) == (b.sa_flags & knownFlags)) &&
        ((a.sa_flags & SA_SIGINFO) ? a.sa_sigaction == b.sa_sigaction
                                   : a.sa_handler == b.sa_handler);
}

void SignalHandler::enable(SignalHandlerCallback &aCallback) noexcept
{
    if (!callback)
    {
        struct sigaction sa = makeHandlerAction();
        for (int signo : handledSignals)
            sigaction(signo, &sa, &getHandlerInfo(signo).action);
        callback = &aCallback;
    }
}

void SignalHandler::disable() noexcept
{
    if (callback)
    {
        callback = nullptr;
        for (int signo : handledSignals)
        {
            auto &handlerInfo = getHandlerInfo(signo);
            struct sigaction sa = {};
            sigaction(signo, nullptr, &sa);
            // Восстанавливаем прежний обработчик, только если всё ещё установлен наш.
            if (sa == makeHandlerAction())
                sigaction(signo, &handlerInfo.action, nullptr);
            handlerInfo.action = makeDefaultAction();
        }
    }
}

SignalHandler::HandlerInfo &SignalHandler::getHandlerInfo(int signo) noexcept
{
    static HandlerInfo infos[HandledSignalCount];
    switch (signo)
    {
        case SIGINT:    return infos[SigInt];
        case SIGQUIT:   return infos[SigQuit];
        case SIGILL:    return infos[SigIll];
        case SIGABRT:   return infos[SigAbrt];
        case SIGBUS:    return infos[SigBus];
        case SIGFPE:    return infos[SigFpe];
        case SIGSEGV:   return infos[SigSegv];
        case SIGPIPE:   return infos[SigPipe];
        case SIGTERM:   return infos[SigTerm];
        case SIGTSTP:   return infos[SigTstp];
        default:        abort();
    }
}

void SignalHandler::handleSignal(int signo, siginfo_t *info, void *context)
{
    // В многопоточном приложении другой поток может изменить обработчик
    // сигнала, пока этот выполняется; повлиять на это невозможно.
    //
    auto &handlerInfo = getHandlerInfo(signo);
    struct sigaction currentAction {};
    SignalHandlerCallback *callback;
    if ((callback = SignalHandler::callback) && handlerInfo.running.exchange(true) == false)
    {
        struct sigaction nextAction = handlerInfo.action;
        // Снимаем текущее действие на случай повторного сигнала во время
        // обратного вызова.
        sigaction(signo, nullptr, &currentAction);
        // Вызываем обратный вызов, который теоретически должен быть безопасен для сигналов.
        callback(true);
        // Устанавливаем и вызываем действие, действовавшее перед установкой
        // нашего обработчика.
        sigaction(signo, &nextAction, nullptr);
        if (invokeHandlerOrDefault(signo, nextAction, info, context))
            // Иногда необходимо выйти из этого обработчика.
            return;
        // Если процесс не завершён, готовимся возобновить обычное выполнение.
        //
        callback(false);
        // Восстанавливаем действие, установленное на момент вызова этого обработчика.
        sigaction(signo, &currentAction, nullptr);
        handlerInfo.running = false;
    }
    else
    {
        // Если обработчик неожиданно вызван повторно либо обратный вызов не
        // задан, вызываем обработчик по умолчанию.
        //
        struct sigaction sa = makeDefaultAction();
        sigaction(signo, &sa, &currentAction);
        if (invokeDefault(signo, info))
            return;
        // Восстанавливаем действие, установленное на момент вызова этого обработчика.
        sigaction(signo, &currentAction, nullptr);
    }
}

template <class T>
static inline bool isCustomHandler(T &&handler)
{
    return (void *) handler != (void *) SIG_DFL
        && (void *) handler != (void *) SIG_IGN;
}

bool SignalHandler::invokeHandlerOrDefault( int signo, const struct sigaction &action,
                                            siginfo_t *info, void *context ) noexcept
{
    // Пользовательский обработчик вызываем напрямую.
    if ((action.sa_flags & SA_SIGINFO) && isCustomHandler(action.sa_sigaction))
        action.sa_sigaction(signo, info, context);
    else if (!(action.sa_flags & SA_SIGINFO) && isCustomHandler(action.sa_handler))
        action.sa_handler(signo);
    else
        // Запускаем обработчик по умолчанию повторной отправкой сигнала.
        return invokeDefault(signo, info);
    return false;
}

bool SignalHandler::invokeDefault(int signo, siginfo_t *info) noexcept
{
    // Выходим из обработчика, позволяя ядру повторно породить синхронный
    // сигнал. Это сохраняет исходный стек, si_addr и другие данные.
    if ( (signo == SIGILL || signo == SIGBUS || signo == SIGFPE || signo == SIGSEGV)
         && info->si_code > 0 )
        return true;
    // Иначе отправляем сигнал вручную.
    sigset_t mask, oldMask;
    sigemptyset(&mask);
    // Разблокируем этот сигнал.
    sigaddset(&mask, signo);
    sigprocmask(SIG_UNBLOCK, &mask, &oldMask);
    raise(signo);
    // Если процесс не завершён, восстанавливаем исходную маску.
    sigprocmask(SIG_SETMASK, &oldMask, nullptr);
    return false;
}

} // пространство имён tvision

#endif // _TV_UNIX
