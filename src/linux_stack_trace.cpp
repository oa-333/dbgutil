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

DbgUtilErr LinuxStackTraceProvider::initialize() {
    // register request handler
    return LinuxThreadManager::getInstance()->registerThreadRequestHandler(
        DBGUTIL_GET_STACK_TRACE_REQUEST, getStackTraceHandler);
}

DbgUtilErr LinuxStackTraceProvider::terminate() {
    return LinuxThreadManager::getInstance()->unregisterThreadRequestHandler(
        DBGUTIL_GET_STACK_TRACE_REQUEST);
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
    unsigned long a[100];
    int ctr = 0;
    unw_word_t ip, sp;

    while (unw_step(&cursor) > 0) {
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        void* addr = (void*)ip;
        listener->onStackFrame(addr);
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxStackTraceProvider::getThreadStackTrace(const ThreadId& threadId,
                                                        RawStackTrace& stackTrace) {
    // for current thread do regular stack walking
    if (threadId.m_osThreadId == OsUtil::getCurrentThreadId()) {
        return getStackTrace(nullptr, stackTrace);
    }

    // otherwise, execute thread request
    DbgUtilErr result = DBGUTIL_ERR_OK;
#ifdef DBGUTIL_MINGW
    return Win32StackTraceProvider::getInstance()->getThreadStackTrace(threadId, stackTrace);
#else
    DbgUtilErr rc = LinuxThreadManager::getInstance()->execThreadRequest(
        threadId, DBGUTIL_GET_STACK_TRACE_REQUEST, (void*)&stackTrace, result);
    if (rc == DBGUTIL_ERR_OK) {
        rc = result;
    }
    return rc;
#endif
}

DbgUtilErr initLinuxStackTrace() {
    LinuxStackTraceProvider::createInstance();
    DbgUtilErr rc = LinuxStackTraceProvider::getInstance()->initialize();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    setStackTraceProvider(LinuxStackTraceProvider::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLinuxStackTrace() {
    setStackTraceProvider(nullptr);
    DbgUtilErr rc = LinuxStackTraceProvider::getInstance()->terminate();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    LinuxStackTraceProvider::destroyInstance();
    return DBGUTIL_ERR_OK;
}

#if 0
BEGIN_STARTUP_JOB(OsStackTrace) {
    LinuxStackTraceProvider::createInstance();
    DbgUtilErr rc = LinuxStackTraceProvider::getInstance()->initialize();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    setStackTraceProvider(LinuxStackTraceProvider::getInstance());
    return DBGUTIL_ERR_OK;
}
END_STARTUP_JOB(OsStackTrace)
DECLARE_STARTUP_JOB_DEP(OsStackTrace, LinuxThreadManager)

BEGIN_TEARDOWN_JOB(OsStackTrace) {
    setStackTraceProvider(nullptr);
    DbgUtilErr rc = LinuxStackTraceProvider::getInstance()->terminate();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    LinuxStackTraceProvider::destroyInstance();
    return DBGUTIL_ERR_OK;
}
END_TEARDOWN_JOB(OsStackTrace)
DECLARE_TEARDOWN_JOB_DEP(OsStackTrace, LinuxThreadManager)
#endif

}  // namespace dbgutil

#endif  // DBGUTIL_GCC