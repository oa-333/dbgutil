#ifndef __WIN32_THREAD_MANAGER_H__
#define __WIN32_THREAD_MANAGER_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

#include "dbgutil_common.h"
#include "os_thread_manager.h"

namespace dbgutil {

class Win32ThreadManager : public OsThreadManager {
public:
    /** @brief Creates the singleton instance of the module manager for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the module manager. */
    static Win32ThreadManager* getInstance();

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

private:
    Win32ThreadManager() {}
    Win32ThreadManager(const Win32ThreadManager&) = delete;
    Win32ThreadManager(Win32ThreadManager&&) = delete;
    Win32ThreadManager& operator=(const Win32ThreadManager&) = delete;
    ~Win32ThreadManager() final {}

    static Win32ThreadManager* sInstance;
};

extern DbgUtilErr initWin32ThreadManager();
extern DbgUtilErr termWin32ThreadManager();

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS

#endif  // __WIN32_THREAD_MANAGER_H__