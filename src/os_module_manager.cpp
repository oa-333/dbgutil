#include "os_module_manager.h"

#include <cassert>

#include "os_module_manager_internal.h"

namespace dbgutil {

static OsModuleManager* sModuleManager = nullptr;

DbgUtilErr OsModuleManager::getModuleByAddress(void* address, OsModuleInfo& moduleInfo) {
    // allow many readers to query for the module together
    {
        std::shared_lock<std::shared_mutex> lock(m_lock);
        OsModuleSet::const_iterator itr = m_moduleSet.lower_bound(address);
        if (itr != m_moduleSet.end() && itr->contains(address)) {
            moduleInfo = *itr;
            return DBGUTIL_ERR_OK;
        }
    }

    // otherwise trigger a system call, but do this outside lock scope
    // this may cause duplicates, but this is a small sacrifice for allowing higher concurrency
    // NOTE: on Unix/Linux systems this may require a full refresh of the list (when querying for
    // symbol in the address space of the main executable image)
    DbgUtilErr rc = getOsModuleByAddress(address, moduleInfo);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // now add module to module set, use write lock
    std::unique_lock<std::shared_mutex> lock(m_lock);
    std::pair<OsModuleSet::iterator, bool> itrRes = m_moduleSet.insert(moduleInfo);
    if (!itrRes.second) {
        // race condition, take the details from the first one who succeeded
        moduleInfo = *itrRes.first;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsModuleManager::getModuleByName(const char* name, OsModuleInfo& moduleInfo,
                                            bool shouldRefreshModuleList /* = false */) {
    DbgUtilErr rc = searchModule(name, moduleInfo);
    if (rc == DBGUTIL_ERR_OK) {
        return rc;
    }

    if (!shouldRefreshModuleList) {
        return DBGUTIL_ERR_NOT_FOUND;
    }

    rc = refreshModuleList();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // run search again
    return searchModule(name, moduleInfo);
}

DbgUtilErr OsModuleManager::getMainModule(OsModuleInfo& moduleInfo) {
    // use read lock for query
    {
        std::shared_lock<std::shared_mutex> lock(m_lock);
        if (m_mainModuleValid) {
            moduleInfo = m_mainModule;
            return DBGUTIL_ERR_OK;
        }
    }

    // get module info outside of lock scope, we don't care about race condition
    DbgUtilErr rc = getOsModuleByAddress(nullptr, moduleInfo);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // cache result and return
    setMainModule(moduleInfo);
    return DBGUTIL_ERR_OK;
}

void OsModuleManager::clearModuleSet() {
    std::unique_lock<std::shared_mutex> lock(m_lock);
    m_moduleSet.clear();
    // main module is not expected to change, so we leave it as is, whether it is already
    // initialized or not
}

void OsModuleManager::addModuleInfo(const OsModuleInfo& moduleInfo) {
    std::unique_lock<std::shared_mutex> lock(m_lock);
    m_moduleSet.insert(moduleInfo);
    // we don't care if succeeded or not, first one to insert wins
}

void OsModuleManager::setMainModule(const OsModuleInfo& moduleInfo) {
    std::unique_lock<std::shared_mutex> lock(m_lock);
    // account for possible race condition, first one wins
    if (!m_mainModuleValid) {
        m_mainModule = moduleInfo;
        m_mainModuleValid = true;
    }
}

DbgUtilErr OsModuleManager::searchModule(const char* name, OsModuleInfo& moduleInfo) {
    // thread-safe search
    {
        std::shared_lock<std::shared_mutex> lock(m_lock);
        for (const OsModuleInfo& modInfo : m_moduleSet) {
            if (modInfo.m_modulePath.find(name) != std::string::npos) {
                moduleInfo = modInfo;
                return DBGUTIL_ERR_OK;
            }
        }
    }
    return DBGUTIL_ERR_NOT_FOUND;
}

void setModuleManager(OsModuleManager* moduleManager) {
    assert((moduleManager != nullptr && sModuleManager == nullptr) ||
           (moduleManager == nullptr && sModuleManager != nullptr));
    sModuleManager = moduleManager;
}

OsModuleManager* getModuleManager() {
    assert(sModuleManager != nullptr);
    return sModuleManager;
}

}  // namespace dbgutil
