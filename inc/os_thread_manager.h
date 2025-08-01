#ifndef __OS_THREAD_MANGER_H__
#define __OS_THREAD_MANGER_H__

#include <cstdint>

#include "libdbg_def.h"
#include "libdbg_err.h"

namespace libdbg {

/** @brief A thread visitor used to traverse all threads. */
class DBGUTIL_API ThreadVisitor {
public:
    virtual ~ThreadVisitor() {}

    /**
     * @brief Handles a visited thread.
     * @param threadId The system thread id.
     * @param osData Any additional OS-specific thread-related information. On Linux this can be
     * cast directly to pthread_t.
     */
    virtual void onThreadId(os_thread_id_t threadId) = 0;

protected:
    ThreadVisitor() {}
    ThreadVisitor(const ThreadVisitor&) = delete;
    ThreadVisitor(ThreadVisitor&&) = delete;
    ThreadVisitor& operator=(const ThreadVisitor&) = delete;
};

class DBGUTIL_API OsThreadManager {
public:
    OsThreadManager(const OsThreadManager&) = delete;
    OsThreadManager(OsThreadManager&&) = delete;
    OsThreadManager& operator=(const OsThreadManager&) = delete;
    virtual ~OsThreadManager() {}

    /**
     * @brief Traverses all running thread ids.
     * @param visitor The thread visitor.
     * @return The operation result.
     */
    virtual LibDbgErr visitThreadIds(ThreadVisitor* visitor) = 0;

protected:
    OsThreadManager() {}
};

/** @brief Installs a thread manager. */
extern DBGUTIL_API void setThreadManager(OsThreadManager* provider);

/** @brief Retrieves the installed thread manager. */
extern DBGUTIL_API OsThreadManager* getThreadManager();

/** @brief Utility API for lambda syntax. */
template <typename F>
inline void visitThreadIds(F f) {
    struct Visitor final : public ThreadVisitor {
        Visitor(F f) : m_f(f) {}
        void onThread(os_thread_id_t threadId) { m_f(threadId); }
        F m_f;
    };
    Visitor visitor(f);
    getThreadManager()->visitThreadIds(&visitor);
}

}  // namespace libdbg

#endif  // __OS_THREAD_MANGER_H__