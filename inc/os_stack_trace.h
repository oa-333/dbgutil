#ifndef __OS_STACK_TRACE_H__
#define __OS_STACK_TRACE_H__

#include <vector>

#include "libdbg_def.h"
#include "libdbg_err.h"

namespace libdbg {

/** @typedef Raw stack trace. */
typedef std::vector<void*> RawStackTrace;

/** @brief A stack frame listener used in conjunction with @ref walkStack. */
class LIBDBG_API StackFrameListener {
public:
    virtual ~StackFrameListener() {}

    /** @brief Handle stack frame (from innermost to outermost). */
    virtual void onStackFrame(void* frameAddress) = 0;

protected:
    StackFrameListener() {}
    StackFrameListener(const StackFrameListener&) = delete;
    StackFrameListener(StackFrameListener&&) = delete;
    StackFrameListener& operator=(StackFrameListener&) = delete;
};

class LIBDBG_API OsStackTraceProvider {
public:
    OsStackTraceProvider(const OsStackTraceProvider&) = delete;
    OsStackTraceProvider(OsStackTraceProvider&&) = delete;
    OsStackTraceProvider& operator=(OsStackTraceProvider&) = delete;
    virtual ~OsStackTraceProvider() {}

    /**
     * @brief Walks the call stack from possibly the given context point.
     * @param listener The stack frame listener.
     * @param context The call context. Pass null to capture current thread call stack.
     * @return LibDbgErr The operation result.
     */
    virtual LibDbgErr walkStack(StackFrameListener* listener, void* context) = 0;

    /**
     * @brief Retrieves stack trace for a specific thread by id.
     * @param threadId The thread id.
     * @param[out] stackTrace The resulting stack trace.
     * @return LibDbgErr The operation result.
     */
    virtual LibDbgErr getThreadStackTrace(os_thread_id_t threadId, RawStackTrace& stackTrace) = 0;

    /**
     * @brief Retrieves stack trace of a thread by context.
     * @param context The call context. Pass null to capture current thread call stack.
     * @param[out] stackTrace The resulting stack trace.
     * @return LibDbgErr The operation result.
     */
    LibDbgErr getStackTrace(void* conetxt, RawStackTrace& stackTrace);

protected:
    OsStackTraceProvider() {}

    // LibDbgErr collectThreadStackTrace;
};

/** @brief Installs a stack trace provider. */
extern LIBDBG_API void setStackTraceProvider(OsStackTraceProvider* provider);

/** @brief Retrieves the installed stack trace provider. */
extern LIBDBG_API OsStackTraceProvider* getStackTraceProvider();

}  // namespace libdbg

#endif  // __OS_STACK_TRACE_H__