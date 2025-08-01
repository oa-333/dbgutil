#include "libdbg_def.h"

#ifdef LIBDBG_GCC

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <cassert>

#include "linux_stack_trace.h"
#include "linux_thread_manager.h"
#include "os_util.h"

#ifdef LIBDBG_MINGW
#include "win32_stack_trace.h"
#endif

#define DBGUTIL_GET_STACK_TRACE_REQUEST 1

namespace libdbg {

LinuxStackTraceProvider* LinuxStackTraceProvider::sInstance = nullptr;

void LinuxStackTraceProvider::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) LinuxStackTraceProvider();
    assert(sInstance != nullptr);
}

LinuxStackTraceProvider* LinuxStackTraceProvider::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void LinuxStackTraceProvider::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

LibDbgErr LinuxStackTraceProvider::walkStack(StackFrameListener* listener, void* context) {
    unw_context_t unw_context;
    if (context == nullptr) {
        unw_getcontext(&unw_context);
        context = &unw_context;
    }

    unw_context_t* ucontext = (unw_context_t*)context;
    unw_cursor_t cursor;
    unw_init_local(&cursor, ucontext);

    while (unw_step(&cursor) > 0) {
        unw_word_t ip = 0;
        unw_word_t sp = 0;
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        void* addr = (void*)ip;
        listener->onStackFrame(addr);
    }
    return LIBDBG_ERR_OK;
}

LibDbgErr LinuxStackTraceProvider::getThreadStackTrace(os_thread_id_t threadId,
                                                       RawStackTrace& stackTrace) {
    // for current thread do regular stack walking
    if (threadId == OsUtil::getCurrentThreadId()) {
        return getStackTrace(nullptr, stackTrace);
    }

    // otherwise, execute thread request
#ifdef LIBDBG_MINGW
    return Win32StackTraceProvider::getInstance()->getThreadStackTrace(threadId, stackTrace);
#else
    class GetStackTraceExecutor : public ThreadExecutor {
    public:
        GetStackTraceExecutor(RawStackTrace& stackTrace) : m_stackTrace(stackTrace) {}
        ~GetStackTraceExecutor() final {}

        LibDbgErr execRequest() final {
            return getStackTraceProvider()->getStackTrace(nullptr, m_stackTrace);
        }

    private:
        RawStackTrace& m_stackTrace;
    };

    GetStackTraceExecutor executor(stackTrace);
    LibDbgErr result = LIBDBG_ERR_OK;
    LibDbgErr rc =
        LinuxThreadManager::getInstance()->execThreadRequest(threadId, &executor, result);
    if (rc == LIBDBG_ERR_OK) {
        rc = result;
    }
    return rc;
#endif
}

LibDbgErr initLinuxStackTrace() {
    LinuxStackTraceProvider::createInstance();
    setStackTraceProvider(LinuxStackTraceProvider::getInstance());
    return LIBDBG_ERR_OK;
}

LibDbgErr termLinuxStackTrace() {
    setStackTraceProvider(nullptr);
    LinuxStackTraceProvider::destroyInstance();
    return LIBDBG_ERR_OK;
}

}  // namespace libdbg

#endif  // LIBDBG_GCC