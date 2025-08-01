#ifndef __LINUX_STACK_TRACE_H__
#define __LINUX_STACK_TRACE_H__

#include "libdbg_def.h"

#ifdef DBGUTIL_GCC

#include "os_stack_trace.h"

namespace libdbg {

class LinuxStackTraceProvider : public OsStackTraceProvider {
public:
    /** @brief Creates the singleton instance of the stack-trace provider for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the stack-trace provider. */
    static LinuxStackTraceProvider* getInstance();

    /** @brief Destroys the singleton instance of the stack-trace provider. */
    static void destroyInstance();

    /**
     * @brief Walks the call stack from possibly the given context point.
     * @param listener The stack frame listener.
     * @param context The call context. Pass null to capture current thread call stack.
     */
    LibDbgErr walkStack(StackFrameListener* listener, void* context) final;

    /**
     * @brief Retrieves stack trace for a specific thread.
     * @param threadId The thread id.
     * @param[out] stackTrace The resulting stack trace.
     * @return LibDbgErr The operation result.
     */
    LibDbgErr getThreadStackTrace(os_thread_id_t threadId, RawStackTrace& stackTrace) final;

private:
    LinuxStackTraceProvider() {}
    ~LinuxStackTraceProvider() final {}

    static LinuxStackTraceProvider* sInstance;
};

extern LibDbgErr initLinuxStackTrace();
extern LibDbgErr termLinuxStackTrace();

}  // namespace libdbg

#endif  // DBGUTIL_GCC

#endif  // __LINUX_STACK_TRACE_H__