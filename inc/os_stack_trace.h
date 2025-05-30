#ifndef __OS_STACK_TRACE_H__
#define __OS_STACK_TRACE_H__

#include <vector>

#include "dbg_util_def.h"
#include "dbg_util_err.h"

namespace dbgutil {

/** @typedef Raw stack trace. */
typedef std::vector<void*> RawStackTrace;

/** @brief A stack frame listener used in conjunction with @ref walkStack. */
class DBGUTIL_API StackFrameListener {
public:
    virtual ~StackFrameListener() {}

    /** @brief Handle stack frame (from innermost to outermost). */
    virtual void onStackFrame(void* frameAddress) = 0;

protected:
    StackFrameListener() {}
};

class DBGUTIL_API OsStackTraceProvider {
public:
    OsStackTraceProvider(const OsStackTraceProvider&) = delete;
    OsStackTraceProvider(OsStackTraceProvider&&) = delete;
    virtual ~OsStackTraceProvider() {}

    /**
     * @brief Walks the call stack from possibly the given context point.
     * @param listener The stack frame listener.
     * @param context The call context. Pass null to capture current thread call stack.
     * @return DbgUtilErr The operation result.
     */
    virtual DbgUtilErr walkStack(StackFrameListener* listener, void* context) = 0;

    /**
     * @brief Retrieves stack trace for a specific thread by id.
     * @param threadId The thread id.
     * @param[out] stackTrace The resulting stack trace.
     * @return DbgUtilErr The operation result.
     */
    virtual DbgUtilErr getThreadStackTrace(os_thread_id_t threadId, RawStackTrace& stackTrace) = 0;

    /**
     * @brief Retrieves stack trace of a thread by context.
     * @param context The call context. Pass null to capture current thread call stack.
     * @param[out] stackTrace The resulting stack trace.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr getStackTrace(void* conetxt, RawStackTrace& stackTrace);

protected:
    OsStackTraceProvider() {}

    // DbgUtilErr collectThreadStackTrace;
};

/** @brief Installs a stack trace provider. */
extern DBGUTIL_API void setStackTraceProvider(OsStackTraceProvider* provider);

/** @brief Retrieves the installed stack trace provider. */
extern DBGUTIL_API OsStackTraceProvider* getStackTraceProvider();

}  // namespace dbgutil

#endif  // __OS_STACK_TRACE_H__