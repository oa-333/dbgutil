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
     * @brief Walks the call stack from possibly the given context point.
     * @param listener The stack frame listener.
     * @param context The call context. Pass null to capture current thread call stack.
     */
    DbgUtilErr visitThreads(ThreadListener* listener) final;

    /**
     * @brief Retrieves thread handle by id.
     * @param threadId The thread id.
     * @param threadHandle The resulting thread handle.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr getThreadHandle(const ThreadId& threadId, pthread_t& threadHandle);

    /** @typedef Request handler type. */
    typedef DbgUtilErr (*RequestHandler)(void* data);

    /**
     * @brief Registers a thread request handler, that will execute posted requests on the
     * destination thread context.
     * @param requestType The request type id.
     * @param handler The handler.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr registerThreadRequestHandler(uint32_t requestType, RequestHandler handler);

    /**
     * @brief Unregisters a thread request handler.
     * @param requestType The request type id.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr unregisterThreadRequestHandler(uint32_t requestType);

    /**
     * @brief Posts a request to be executed by the destination thread (non-blocking call).
     * @param threadId The destination thread id.
     * @param requestType The request type id.
     * @param data The request data.
     * @param[out] requestResult The request execution result in case a request was pending.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr execThreadRequest(const ThreadId& threadId, uint32_t requestType, void* requestData,
                                 DbgUtilErr& requestResult);

private:
    LinuxThreadManager() {}
    ~LinuxThreadManager() {}

    static LinuxThreadManager* sInstance;
};

extern DbgUtilErr initLinuxThreadManager();
extern DbgUtilErr termLinuxThreadManager();

}  // namespace dbgutil

#endif  // DBGUTIL_GCC

#endif  // __LINUX_THREAD_MANAGER_H__