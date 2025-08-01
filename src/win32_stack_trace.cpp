#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

#ifdef DBGUTIL_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cassert>

#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "win32_stack_trace.h"
#include "win32_symbol_engine.h"

namespace libdbg {

static Logger sLogger;

Win32StackTraceProvider* Win32StackTraceProvider::sInstance = nullptr;

void Win32StackTraceProvider::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) Win32StackTraceProvider();
    assert(sInstance != nullptr);
}

Win32StackTraceProvider* Win32StackTraceProvider::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void Win32StackTraceProvider::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

DbgUtilErr Win32StackTraceProvider::walkStack(StackFrameListener* listener, void* context) {
    CONTEXT osContext;
    if (context == nullptr) {
        ZeroMemory(&osContext, sizeof(CONTEXT));
        RtlCaptureContext(&osContext);
        context = &osContext;
    }
    return Win32SymbolEngine::getInstance()->walkStack(listener, context);
}

DbgUtilErr Win32StackTraceProvider::getThreadStackTrace(os_thread_id_t threadId,
                                                        RawStackTrace& stackTrace) {
    // check for current thread id (because we cannot suspend current thread)
    if (threadId == GetCurrentThreadId()) {
        return getStackTrace(nullptr, stackTrace);
    }

    // open thread handle
    HANDLE hThread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, threadId);
    if (hThread == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, OpenThread, "Failed to open thread with id %" PRItid, threadId);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // suspend thread
    if (SuspendThread(hThread) == (DWORD)-1) {
        LOG_WIN32_ERROR(sLogger, OpenThread, "Failed to suspend thread %" PRItid, threadId);
        CloseHandle(hThread);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // get thread context
    CONTEXT context = {};
    context.ContextFlags = CONTEXT_ALL;
    if (!GetThreadContext(hThread, &context)) {
        LOG_WIN32_ERROR(sLogger, GetThreadContext, "Failed to get thread %" PRItid " context",
                        threadId);
        ResumeThread(hThread);
        CloseHandle(hThread);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // now get stack trace by context
    DbgUtilErr rc = getStackTrace(&context, stackTrace);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to get stack trace of thread %" PRItid, threadId);
        ResumeThread(hThread);
        CloseHandle(hThread);
        return rc;
    }

    if (ResumeThread(hThread) == (DWORD)-1) {
        LOG_WIN32_ERROR(sLogger, ResumeThread, "Failed to resume of thread %" PRItid, threadId);
        CloseHandle(hThread);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    if (!CloseHandle(hThread)) {
        LOG_WIN32_ERROR(sLogger, CloseHandle, "Failed to close thread %" PRItid " handle",
                        threadId);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr initWin32StackTrace() {
    registerLogger(sLogger, "win32_stack_trace");
    Win32StackTraceProvider::createInstance();
#ifdef DBGUTIL_MSVC
    setStackTraceProvider(Win32StackTraceProvider::getInstance());
#endif
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termWin32StackTrace() {
#ifdef DBGUTIL_MSVC
    setStackTraceProvider(nullptr);
#endif
    Win32StackTraceProvider::destroyInstance();
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

}  // namespace libdbg

#endif  // DBGUTIL_WINDOWS