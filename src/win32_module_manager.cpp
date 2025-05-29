#include "win32_module_manager.h"

#ifdef DBGUTIL_WINDOWS

// #include <dbghelp.h>
#include <psapi.h>

#include <cassert>
#include <vector>

#include "dbgutil_log_imp.h"

namespace dbgutil {

static Logger sLogger;

Win32ModuleManager* Win32ModuleManager::sInstance = nullptr;

DbgUtilErr Win32ModuleManager::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) Win32ModuleManager();
    assert(sInstance != nullptr);
    return sInstance->initProcessHandle();
}

Win32ModuleManager* Win32ModuleManager::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void Win32ModuleManager::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

DbgUtilErr Win32ModuleManager::refreshModuleList() {
    // figure out how many entries are needed
    DWORD bytesNeeded = 0;
    if (!EnumProcessModules(m_processHandle, nullptr, 0, &bytesNeeded)) {
        LOG_SYS_ERROR(sLogger, EnumProcessModules, "Failed to enumerate process modules");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // allocate enough entries and get the module list
    uint32_t moduleCount = bytesNeeded / sizeof(HMODULE);
    std::vector<HMODULE> moduleHandles(moduleCount);
    if (!EnumProcessModules(m_processHandle, &moduleHandles[0], moduleCount * sizeof(HMODULE),
                            &bytesNeeded)) {
        LOG_SYS_ERROR(sLogger, EnumProcessModules,
                      "Failed to enumerate process modules (second time)");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // some modules may be loaded/unloaded manually so we clear the module set before adding the
    // modules one by one
    clearModuleSet();

    // now for each module get its info
    OsModuleInfo mainModule;
    for (uint32_t i = 0; i < moduleCount; ++i) {
        HMODULE module = moduleHandles[i];
        OsModuleInfo moduleInfo;
        DbgUtilErr rc = getOsModuleInfo(module, moduleInfo);
        if (rc != DBGUTIL_ERR_OK) {
            LOG_ERROR(sLogger, "Failed to get module information");
            return rc;
        }

        addModuleInfo(moduleInfo);

        if (i == 0) {
            mainModule = moduleInfo;
        }
    }

    // record the main module
    setMainModule(mainModule);
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32ModuleManager::getOsModuleByAddress(void* address, OsModuleInfo& moduleInfo) {
    HMODULE mod = nullptr;
    if (address == nullptr) {
        mod = GetModuleHandleA(nullptr);
        if (mod == nullptr) {
            LOG_SYS_ERROR(sLogger, GetModuleHandle,
                          "Failed to get module handle for current process");
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
    } else {
        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCSTR)address, &mod)) {
            LOG_SYS_ERROR(sLogger, GetModuleHandleExA, "Failed to get module for address %p",
                          address);
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
    }

    return getOsModuleInfo(mod, moduleInfo);
}

Win32ModuleManager::Win32ModuleManager() : m_processHandle(INVALID_HANDLE_VALUE) {}

Win32ModuleManager::~Win32ModuleManager() {
    if (m_processHandle != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(m_processHandle)) {
            LOG_SYS_ERROR(
                sLogger, CloseHandle,
                "Cannot terminate symbol handler: failed to close current process handle");
        }
        m_processHandle = INVALID_HANDLE_VALUE;
    }
}

DbgUtilErr Win32ModuleManager::initProcessHandle() {
    // open current process handle
    m_processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    if (m_processHandle == INVALID_HANDLE_VALUE) {
        LOG_SYS_ERROR(sLogger, OpenProcess,
                      "Cannot initialize symbol handler: failed to open current process handle");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32ModuleManager::getOsModuleInfo(HMODULE module, OsModuleInfo& moduleInfo) {
    MODULEINFO modInfo;
    if (!GetModuleInformation(m_processHandle, module, &modInfo, sizeof(MODULEINFO))) {
        LOG_SYS_ERROR(sLogger, GetModuleInformation, "Failed to get module %p information",
                      (void*)module);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    moduleInfo.m_loadAddress = modInfo.lpBaseOfDll;
    moduleInfo.m_size = modInfo.SizeOfImage;

    char modulePath[MAX_PATH];
    DWORD pathLen = GetModuleFileNameExA(m_processHandle, module, modulePath, MAX_PATH);
    if (pathLen == 0) {
        LOG_SYS_ERROR(sLogger, GetModuleFileNameExA, "Failed to get module %p file name",
                      (void*)module);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    moduleInfo.m_modulePath = modulePath;

    LOG_TRACE(sLogger, "Loaded module %s at %p", moduleInfo.m_modulePath.c_str(),
              moduleInfo.m_loadAddress);

    return DBGUTIL_ERR_OK;
}

DbgUtilErr initWin32ModuleManager() {
    registerLogger(sLogger, "win32_module_manager");
    DbgUtilErr rc = Win32ModuleManager::createInstance();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    setModuleManager(Win32ModuleManager::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termWin32ModuleManager() {
    setModuleManager(nullptr);
    Win32ModuleManager::destroyInstance();
    return DBGUTIL_ERR_OK;
}

/*BEGIN_STARTUP_JOB(OsModuleManager) {
    DbgUtilErr rc = Win32ModuleManager::createInstance();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    setModuleManager(Win32ModuleManager::getInstance());
    return DBGUTIL_ERR_OK;
}
END_STARTUP_JOB(OsModuleManager)

BEGIN_TEARDOWN_JOB(OsModuleManager) {
    setModuleManager(nullptr);
    Win32ModuleManager::destroyInstance();
    return DBGUTIL_ERR_OK;
}
END_TEARDOWN_JOB(OsModuleManager)*/

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS