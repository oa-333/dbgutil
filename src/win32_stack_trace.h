#ifndef __WIN32_STACK_TRACE_H__
#define __WIN32_STACK_TRACE_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

#include "os_stack_trace.h"

namespace dbgutil {

class Win32StackTraceProvider : public OsStackTraceProvider {
public:
    /** @brief Creates the singleton instance of the stack-trace provider for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the stack-trace provider. */
    static Win32StackTraceProvider* getInstance();

    /** @brief Destroys the singleton instance of the stack-trace provider. */
    static void destroyInstance();

    /**
     * @brief Walks the call stack from possibly the given context point.
     * @param listener The stack frame listener.
     * @param context The call context. Pass null to capture current thread call stack.
     */
    DbgUtilErr walkStack(StackFrameListener* listener, void* context) final;

    /**
     * @brief Retrieves stack trace for a specific thread.
     * @param threadId The thread id.
     * @param[out] stackTrace The resulting stack trace.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr getThreadStackTrace(os_thread_id_t threadId, RawStackTrace& stackTrace) final;

private:
    Win32StackTraceProvider() {}
    Win32StackTraceProvider(const Win32StackTraceProvider&) = delete;
    Win32StackTraceProvider(Win32StackTraceProvider&&) = delete;
    Win32StackTraceProvider& operator=(const Win32StackTraceProvider&) = delete;
    ~Win32StackTraceProvider() final {}

    static Win32StackTraceProvider* sInstance;
};

extern DbgUtilErr initWin32StackTrace();
extern DbgUtilErr termWin32StackTrace();

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS

#endif  // __WIN32_STACK_TRACE_H__