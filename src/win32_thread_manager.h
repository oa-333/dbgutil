#ifndef __WIN32_THREAD_MANAGER_H__
#define __WIN32_THREAD_MANAGER_H__

#include "libdbg_def.h"

#ifdef LIBDBG_WINDOWS

#include "dbgutil_common.h"
#include "os_thread_manager.h"

namespace libdbg {

class Win32ThreadManager : public OsThreadManager {
public:
    /** @brief Creates the singleton instance of the module manager for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the module manager. */
    static Win32ThreadManager* getInstance();

    /** @brief Destroys the singleton instance of the module manager. */
    static void destroyInstance();

    /** @brief Initializes the symbol engine. */
    LibDbgErr initialize();

    /** @brief Destroys the symbol engine. */
    LibDbgErr terminate();

    /**
     * @brief Traverses all running threads.
     * @param visitor The thread visitor.
     * @return The operation result.
     */
    LibDbgErr visitThreadIds(ThreadVisitor* visitor) final;

private:
    Win32ThreadManager() {}
    Win32ThreadManager(const Win32ThreadManager&) = delete;
    Win32ThreadManager(Win32ThreadManager&&) = delete;
    Win32ThreadManager& operator=(const Win32ThreadManager&) = delete;
    ~Win32ThreadManager() {}

    static Win32ThreadManager* sInstance;
};

extern LibDbgErr initWin32ThreadManager();
extern LibDbgErr termWin32ThreadManager();

}  // namespace libdbg

#endif  // LIBDBG_WINDOWS

#endif  // __WIN32_THREAD_MANAGER_H__