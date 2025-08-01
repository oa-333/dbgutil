#ifndef __LINUX_THREAD_MANAGER_H__
#define __LINUX_THREAD_MANAGER_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_GCC

#include <pthread.h>

#include "dbgutil_common.h"
#include "os_thread_manager.h"

namespace libdbg {

/** @brief An active executor used for executing some operation on a target thread context. */
class ThreadExecutor {
public:
    virtual ~ThreadExecutor() {}

    virtual DbgUtilErr execRequest() = 0;

protected:
    ThreadExecutor() {}
};

class LinuxThreadManager : public OsThreadManager {
public:
    /** @brief Creates the singleton instance of the module manager for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the module manager. */
    static LinuxThreadManager* getInstance();

    /** @brief Destroys the singleton instance of the module manager. */
    static void destroyInstance();

    /** @brief Initializes the symbol engine. */
    DbgUtilErr initialize();

    /** @brief Destroys the symbol engine. */
    DbgUtilErr terminate();

    /**
     * @brief Traverses all running threads.
     * @param visitor The thread visitor.
     * @return The operation result.
     */
    DbgUtilErr visitThreadIds(ThreadVisitor* visitor) final;

    /**
     * @brief Retrieves thread handle by id.
     * @param threadId The thread id.
     * @param threadHandle The resulting thread handle.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr getThreadHandle(os_thread_id_t threadId, pthread_t& threadHandle);

    /**
     * @brief Requests to execute an operation on another thread (blocking call).
     * @param threadId The destination thread id.
     * @param executor The request executor.
     * @param[out] requestResult The request execution result (valid only if entire function
     * result is ok).
     * @return DbgUtilErr The operation result. This refers only to the ability to post the
     * operation to be executed on the target thread, and then collecting the result. The actual
     * result of the operation being executed on the target thread, is returned via the @ref
     * opResult out parameter.
     */
    DbgUtilErr execThreadRequest(os_thread_id_t threadId, ThreadExecutor* executor,
                                 DbgUtilErr& requestResult);

private:
    LinuxThreadManager() {}
    ~LinuxThreadManager() {}

    static LinuxThreadManager* sInstance;
};

extern DbgUtilErr initLinuxThreadManager();
extern DbgUtilErr termLinuxThreadManager();

}  // namespace libdbg

#endif  // DBGUTIL_GCC

#endif  // __LINUX_THREAD_MANAGER_H__