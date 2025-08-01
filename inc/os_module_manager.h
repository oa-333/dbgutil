#ifndef __OS_MODULE_MANAGER_H__
#define __OS_MODULE_MANAGER_H__

#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>

#include "libdbg_def.h"
#include "libdbg_err.h"

namespace libdbg {

// the main interface with this service, according to existing use cases, is as follows:
//
// 1. query the module of symbol by address (for basic symbol info).
// 2. make sure symbol module is loaded before symbol data is retrieved (windows only)
// 3. query module data for a symbol by address, for image  reader. this is required both for
// getting symbol table fro the image (MinGW and Unix/Linux), and also for debug sections retrieval
// (dwarf only).
//
// The main idea is to use lazy-load as much as possible.
// for this a lock is needed.

/** @brief Loaded module information. */
struct LIBDBG_API OsModuleInfo {
    OsModuleInfo(const char* modulePath = "", void* loadAddress = nullptr, uint64_t size = 0,
                 void* osData = nullptr)
        : m_modulePath(modulePath), m_loadAddress(loadAddress), m_size(size), m_osData(osData) {}
    OsModuleInfo(const OsModuleInfo&) = default;
    OsModuleInfo(OsModuleInfo&&) = delete;
    OsModuleInfo& operator=(const OsModuleInfo&) = default;
    ~OsModuleInfo() {}

    // NOTE: using std::string here will cause copy and trigger memory allocation, on the other hand
    // using character array will inflate the object size, and on each platform the limit is
    // different (Windows has 32K limit) - for now we leave it as a string
    /** @var The full module path on disk. */
    std::string m_modulePath;

    /** @var The load address of the module. */
    void* m_loadAddress;

    /** @var The size in memory of the module. */
    uint64_t m_size;

    /** @var Any extra data required by underlying OS implementation. */
    void* m_osData;

    /** @brief Computes the module end address. */
    inline void* to() const { return (void*)(((uint64_t)m_loadAddress) + m_size); }

    /** @brief Compares two module info objects by load address. */
    inline bool operator<(const OsModuleInfo& moduleInfo) const {
        return m_loadAddress < moduleInfo.m_loadAddress;
    }

    /** @brief Compares a module info object with an address. */
    inline bool operator<(void* address) const {
        return ((uint64_t)m_loadAddress) + m_size <= ((uint64_t)address);
    }

    /** @brief Queries whether a module info object contains an address. */
    inline bool contains(void* address) const {
        return ((uint64_t)address) >= ((uint64_t)m_loadAddress) &&
               ((uint64_t)address) < (((uint64_t)m_loadAddress) + m_size);
    }

    /** @brief Clears all object members. */
    inline void clear() {
        m_modulePath.clear();
        m_loadAddress = nullptr;
        m_size = 0;
        m_osData = nullptr;
    }
};

/** @brief Manages the list of modules loaded by the process, mostly for debugging purposes. */
class LIBDBG_API OsModuleManager {
public:
    OsModuleManager(const OsModuleManager&) = delete;
    OsModuleManager(OsModuleManager&&) = delete;
    OsModuleManager& operator=(const OsModuleManager&) = delete;
    virtual ~OsModuleManager() {}

    /**
     * @brief Searches for the module containing the given address.
     * @note If the module is not found then a system call is triggered.
     * @param address The address to search.
     * @param[out] moduleInfo The resulting module information.
     * @return LibDbgErr The operation result.
     */
    virtual LibDbgErr getModuleByAddress(void* address, OsModuleInfo& moduleInfo);

    /**
     * @brief Searches for a module by name.
     * @param name The name of the module. Could be just file name without full path.
     * @param[out] moduleInfo The resulting module information.
     * @param shouldRefreshModuleList Optionally specifies whether the module should be refreshed if
     * the module was not found (by default no refreshing takes place).
     * @return LibDbgErr The operation result.
     */
    LibDbgErr getModuleByName(const char* name, OsModuleInfo& moduleInfo,
                              bool shouldRefreshModuleList = false);

    /** @brief Queries for the main executable module of the current process. */
    LibDbgErr getMainModule(OsModuleInfo& moduleInfo);

    /**
     * @brief Traverses loaded modules. Consider calling @ref refreshModuleList() to traverse an
     * up-to-date list of modules.
     * @tparam F The visitor function type.
     * @param f The visitor function. The expected signature is "Error Code func(const OsModuleInfo
     * moduleInfo, bool& shouldStop)". If the visitor function returns error code the module
     * traversal stops, and the error code is returned to the caller. If the shouldStop flag is set
     * to true by the visitor, then the module traversal stops, and E_OK is returned to the caller.
     * @return LibDbgErr
     */
    template <typename F>
    inline LibDbgErr forEachModule(F f) {
        std::shared_lock<std::shared_mutex> lock(m_lock);
        for (const OsModuleInfo& moduleInfo : m_moduleSet) {
            bool shouldStop = false;
            LibDbgErr rc = f(moduleInfo, shouldStop);
            if (rc != LIBDBG_ERR_OK) {
                return rc;
            }
            if (shouldStop) {
                break;
            }
        }
        return LIBDBG_ERR_OK;
    }

    /** @brief Refreshes the module list. */
    virtual LibDbgErr refreshModuleList() = 0;

protected:
    OsModuleManager() : m_mainModuleValid(false) {}

    /**
     * @brief Searches for the module containing the given address (OS-specific implementation).
     * @param address The address to search. Pass null address for retrieving main module
     * information.
     * @param[out] moduleInfo The resulting module information.
     * @return OsModuleInfo* The module containing the address, or null if none was found.
     */
    virtual LibDbgErr getOsModuleByAddress(void* address, OsModuleInfo& moduleInfo) = 0;

    /** @brief Clears the module set. */
    void clearModuleSet();

    /** @brief Adds a module-info object to the module set. */
    void addModuleInfo(const OsModuleInfo& moduleInfo);

    /** @brief Sets the main module information. */
    void setMainModule(const OsModuleInfo& moduleInfo);

private:
    std::shared_mutex m_lock;

    // NOTE: using transparent comparator so we can compare module info with address
    typedef std::set<OsModuleInfo, std::less<>> OsModuleSet;
    OsModuleSet m_moduleSet;

    bool m_mainModuleValid;
    OsModuleInfo m_mainModule;

    LibDbgErr searchModule(const char* name, OsModuleInfo& moduleInfo);
};

/** @brief Installs a module manager implementation. */
extern LIBDBG_API void setModuleManager(OsModuleManager* moduleManager);

/** @brief Retrieves the installed module manager implementation. */
extern LIBDBG_API OsModuleManager* getModuleManager();

}  // namespace libdbg

#endif  // __OS_MODULE_MANAGER_H__