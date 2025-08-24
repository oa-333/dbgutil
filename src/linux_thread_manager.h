#ifndef __LINUX_THREAD_MANAGER_H__
#define __LINUX_THREAD_MANAGER_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_GCC

#include <pthread.h>

#include "dbgutil_common.h"
#include "os_thread_manager.h"

namespace dbgutil {

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

private:
    LinuxThreadManager() {}
    ~LinuxThreadManager() final {}

    static LinuxThreadManager* sInstance;
};

extern DbgUtilErr initLinuxThreadManager();
extern DbgUtilErr termLinuxThreadManager();

}  // namespace dbgutil

#endif  // DBGUTIL_GCC

#endif  // __LINUX_THREAD_MANAGER_H__