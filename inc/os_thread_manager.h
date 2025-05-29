#ifndef __OS_THREAD_MANGER_H__
#define __OS_THREAD_MANGER_H__

#include <cstdint>

#include "dbg_util_def.h"
#include "dbg_util_err.h"

namespace dbgutil {

/** @brief A thread listener used to traverse all threads. */
class DBGUTIL_API ThreadListener {
public:
    virtual ~ThreadListener() {}

    /**
     * @brief Handles a visited thread.
     * @param threadId The system thread id.
     * @param osData Any additional OS-specific thread-related information. On Linux this can be
     * cast directly to pthread_t.
     */
    virtual void onThread(const ThreadId& threadId) = 0;

protected:
    ThreadListener() {}
};

class DBGUTIL_API OsThreadManager {
public:
    OsThreadManager(const OsThreadManager&) = delete;
    OsThreadManager(OsThreadManager&&) = delete;
    virtual ~OsThreadManager() {}

    /**
     * @brief Walks the call stack from possibly the given context point.
     * @param listener The stack frame listener.
     * @param context The call context. Pass null to capture current thread call stack.
     */
    virtual DbgUtilErr visitThreads(ThreadListener* listener) = 0;

protected:
    OsThreadManager() {}
};

/** @brief Installs a thread manager. */
extern DBGUTIL_API void setThreadManager(OsThreadManager* provider);

/** @brief Retrieves the installed thread manager. */
extern DBGUTIL_API OsThreadManager* getThreadManager();

}  // namespace dbgutil

#endif  // __OS_THREAD_MANGER_H__