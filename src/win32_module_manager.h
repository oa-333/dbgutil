#ifndef __WIN32_MODULE_MANAGER_H__
#define __WIN32_MODULE_MANAGER_H__

#include "libdbg_common.h"

#ifdef LIBDBG_WINDOWS
#include "libdbg_log_imp.h"
#include "os_module_manager.h"

#ifdef LIBDBG_MINGW
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace libdbg {

class Win32ModuleManager : public OsModuleManager {
public:
    /** @brief Creates the singleton instance of the module manager for Windows platform. */
    static LibDbgErr createInstance();

    /** @brief Retrieves a reference to the single instance of the module manager. */
    static Win32ModuleManager* getInstance();

    /** @brief Destroys the singleton instance of the module manager. */
    static void destroyInstance();

    /** @brief Refreshes the module list. */
    LibDbgErr refreshModuleList() final;

    inline HANDLE getProcessHandle() { return m_processHandle; }

protected:
    /**
     * @brief Searches for the module containing the given address (OS-specific implementation).
     * @param address The address to search.
     * @param[out] moduleInfo The resulting module information.
     * @return OsModuleInfo* The module containing the address, or null if none was found.
     */
    LibDbgErr getOsModuleByAddress(void* address, OsModuleInfo& moduleInfo) final;

private:
    Win32ModuleManager();
    Win32ModuleManager(const Win32ModuleManager&) = delete;
    Win32ModuleManager(Win32ModuleManager&&) = delete;
    Win32ModuleManager& operator=(const Win32ModuleManager&) = delete;
    ~Win32ModuleManager();

    static Win32ModuleManager* sInstance;

    HANDLE m_processHandle;

    LibDbgErr initProcessHandle();

    LibDbgErr getOsModuleInfo(HMODULE module, OsModuleInfo& moduleInfo);
};

extern LibDbgErr initWin32ModuleManager();
extern LibDbgErr termWin32ModuleManager();

}  // namespace libdbg

#endif  // LIBDBG_WINDOWS

#endif  // __WIN32_MODULE_MANAGER_H__