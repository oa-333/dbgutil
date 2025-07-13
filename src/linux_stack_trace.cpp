#include "dbg_util_def.h"

#ifdef DBGUTIL_GCC

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <cassert>

#include "linux_stack_trace.h"
#include "linux_thread_manager.h"
#include "os_util.h"

#ifdef DBGUTIL_MINGW
#include "win32_stack_trace.h"
#endif

#define DBGUTIL_GET_STACK_TRACE_REQUEST 1

namespace dbgutil {

LinuxStackTraceProvider* LinuxStackTraceProvider::sInstance = nullptr;

static DbgUtilErr getStackTraceHandler(void* data) {
    RawStackTrace* stackTrace = (RawStackTrace*)data;
    return getStackTraceProvider()->getStackTrace(nullptr, *stackTrace);
}

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

DbgUtilErr LinuxStackTraceProvider::walkStack(StackFrameListener* listener, void* context) {
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
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxStackTraceProvider::getThreadStackTrace(os_thread_id_t threadId,
                                                        RawStackTrace& stackTrace) {
    // for current thread do regular stack walking
    if (threadId == OsUtil::getCurrentThreadId()) {
        return getStackTrace(nullptr, stackTrace);
    }

    // otherwise, execute thread request
#ifdef DBGUTIL_MINGW
    return Win32StackTraceProvider::getInstance()->getThreadStackTrace(threadId, stackTrace);
#else
    class GetStackTraceExecutor : public ThreadExecutor {
    public:
        GetStackTraceExecutor(RawStackTrace& stackTrace) : m_stackTrace(stackTrace) {}
        ~GetStackTraceExecutor() final {}

        DbgUtilErr execRequest() final {
            return getStackTraceProvider()->getStackTrace(nullptr, m_stackTrace);
        }

    private:
        RawStackTrace& m_stackTrace;
    };

    GetStackTraceExecutor executor(stackTrace);
    DbgUtilErr result = DBGUTIL_ERR_OK;
    DbgUtilErr rc =
        LinuxThreadManager::getInstance()->execThreadRequest(threadId, &executor, result);
    if (rc == DBGUTIL_ERR_OK) {
        rc = result;
    }
    return rc;
#endif
}

DbgUtilErr initLinuxStackTrace() {
    LinuxStackTraceProvider::createInstance();
    setStackTraceProvider(LinuxStackTraceProvider::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLinuxStackTrace() {
    setStackTraceProvider(nullptr);
    LinuxStackTraceProvider::destroyInstance();
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil

#endif  // DBGUTIL_GCC