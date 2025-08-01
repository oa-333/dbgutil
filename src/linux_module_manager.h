#ifndef __LINUX_MODULE_MANAGER_H__
#define __LINUX_MODULE_MANAGER_H__

#include "dbgutil_common.h"

#ifdef DBGUTIL_LINUX
#include "dbgutil_log_imp.h"
#include "os_module_manager.h"

namespace libdbg {

class LinuxModuleManager : public OsModuleManager {
public:
    /** @brief Creates the singleton instance of the module manager for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the module manager. */
    static LinuxModuleManager* getInstance();

    /** @brief Destroys the singleton instance of the module manager. */
    static void destroyInstance();

    /** @brief Refreshes the module list. */
    LibDbgErr refreshModuleList() final;

protected:
    /**
     * @brief Searches for the module containing the given address (OS-specific implementation).
     * @param address The address to search.
     * @param[out] moduleInfo The resulting module information.
     * @return OsModuleInfo* The module containing the address, or null if none was found.
     */
    LibDbgErr getOsModuleByAddress(void* address, OsModuleInfo& moduleInfo) final;

private:
    LinuxModuleManager();
    ~LinuxModuleManager() {}

    static LinuxModuleManager* sInstance;

    /** @brief Refreshes the module list. */
    LibDbgErr refreshOsModuleList(void* address = nullptr, OsModuleInfo* moduleInfo = nullptr);

    LibDbgErr getCurrentProcessImagePath(std::string& path);

    LibDbgErr parseProcLine(std::string& line, std::string& imagePath, uint64_t& addrLo,
                            uint64_t& addrHi);
};

extern LibDbgErr initLinuxModuleManager();
extern LibDbgErr termLinuxModuleManager();

}  // namespace libdbg

#endif  // DBGUTIL_LINUX

#endif  // __LINUX_MODULE_MANAGER_H__