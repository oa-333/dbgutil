#include "linux_module_manager.h"

#ifdef DBGUTIL_LINUX
#include <dlfcn.h>

#include <cstdio>
#include <iterator>
#include <sstream>

#include "app_life_cycle.h"
#include "dbgutil_log_imp.h"
#include "os_util.h"

// general note regarding implementation
// =====================================
// although it is possible to retrieve loaded module information by calling dl_iterate_phdr()
// and it seems a better solution than parsing /proc/self/maps text, a decision was made in favor of
// the latter. The reason is that the output of dl_iterate_phdr() is incoherent, with zero-sized
// segments, overlapping segments, and most importantly, the overall merge of all segments does not
// match the information coming from /proc/self/maps.

// TODO: this implementation works also on MinGW, is there any benefit?
// should we use this module manager under MSYSTEM?
// can it co-exist with Win32ModuleManager (but only one of them is registered as the
// OsModuleManager)?

namespace dbgutil {

LinuxModuleManager* LinuxModuleManager::sInstance = nullptr;

void LinuxModuleManager::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) LinuxModuleManager();
    assert(sInstance != nullptr);
}

LinuxModuleManager* LinuxModuleManager::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void LinuxModuleManager::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

LinuxModuleManager::LinuxModuleManager() {
    m_logger = elog::ELogSystem::getSharedLogger("csi.common.linux_module_manager");
}

DbgUtilErr LinuxModuleManager::refreshModuleList() { return refreshOsModuleList(); }

DbgUtilErr LinuxModuleManager::getOsModuleByAddress(void* address, OsModuleInfo& moduleInfo) {
    // when trying to get main process module details we can only parse /proc/self/maps
    // when trying to get some shared object file, we can use dladdr() call, but that will only get
    // module base address and image path, but module total size is still missing, so we still need
    // to parse /proc/self/maps
    // so, in any case we need to parse /proc/self/maps, so we just trigger a full module refresh
    // we still might use dladdr to get symbol name, but that does not always work

    // NOTE: module size is a but misleading, because a module may be spanned along several
    // non-contiguous segments. nevertheless we provided the total range bounds.
    return refreshOsModuleList(address, &moduleInfo);
}

DbgUtilErr LinuxModuleManager::refreshOsModuleList(void* address /* = nullptr */,
                                                   OsModuleInfo* moduleInfo /* = nullptr */) {
    // TODO: function too long, and also API is not good
    // parse /proc/self/maps
    std::vector<std::string> lines;
    DbgUtilErr rc = OsUtil::readEntireFileToLines("/proc/self/maps", lines);
    if (rc != DBGUTIL_ERR_OK) {
        ELOG_DEBUG_EX(m_logger, "Failed to read /proc/self/maps: %s", errorCodeToStr(rc));
        return rc;
    }

    // get current process executable image path
    std::string mainImagePath;
    rc = getCurrentProcessImagePath(mainImagePath);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // process each line
    std::string imagePath;
    uint64_t addrLo = 0;
    uint64_t addrHi = 0;

    // we must aggregate into a map by name, because each line provides a segment of addresses
    typedef std::unordered_map<std::string, OsModuleInfo> ModuleMap;
    ModuleMap moduleMap;

    for (std::string& line : lines) {
        ELOG_DEBUG_EX(m_logger, "Processing proc-maps line: %s", line.c_str());
        DbgUtilErr rc = parseProcLine(line, imagePath, addrLo, addrHi);
        if (rc == DBGUTIL_ERR_NOT_FOUND) {
            continue;
        }
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        ELOG_DEBUG_EX(m_logger, "Collected module info: %p-%p %s", (void*)addrLo, (void*)addrHi,
                      imagePath.c_str());

        std::pair<ModuleMap::iterator, bool> itrRes = moduleMap.insert(ModuleMap::value_type(
            imagePath, OsModuleInfo(imagePath.c_str(), (void*)addrLo, addrHi - addrLo)));
        if (itrRes.second) {
            continue;
        }

        // update base address and size (merge ranges, ignore any "holes")
        const OsModuleInfo& modInfo = itrRes.first->second;
        uint64_t currLo = (uint64_t)modInfo.m_loadAddress;
        uint64_t currHi = currLo + modInfo.m_size;
        uint64_t newLo = std::min(currLo, addrLo);
        uint64_t size = std::max(currHi, addrHi) - newLo;
        ELOG_DEBUG_EX(m_logger, "Merged module info: %p-%p %s", (void*)newLo, (void*)(newLo + size),
                      imagePath.c_str());

        // we need to remove the module and insert a new one instead
        moduleMap.erase(itrRes.first);
        bool res = moduleMap
                       .insert(ModuleMap::value_type(
                           imagePath, OsModuleInfo(imagePath.c_str(), (void*)newLo, size)))
                       .second;
        if (!res) {
            return DBGUTIL_ERR_INTERNAL_ERROR;
        }
    }

    // some modules may be loaded/unloaded manually so we clear the module set before adding the
    // modules one by one
    clearModuleSet();

    // now add all modules one by one
    bool moduleFound = false;
    for (const auto& entry : moduleMap) {
        const OsModuleInfo& modInfo = entry.second;
        ELOG_DEBUG_EX(m_logger, "Adding module info: %p-%p %s", modInfo.m_loadAddress, modInfo.to(),
                      modInfo.m_modulePath.c_str());
        addModuleInfo(modInfo);
        if (moduleInfo != nullptr && address != nullptr && modInfo.contains(address)) {
            *moduleInfo = modInfo;
            moduleFound = true;
        }

        if (modInfo.m_modulePath.compare(mainImagePath) == 0) {
            setMainModule(modInfo);
            if (moduleInfo != nullptr && address == nullptr) {
                *moduleInfo = modInfo;
            }
        }
    }

    if (!moduleFound) {
        // it has been observed that at times, the /proc/self/maps does not provide a full list of
        // loaded modules, so in this case we resort to dladdr()
        Dl_info dlinfo = {};
        if (dladdr(address, &dlinfo) == 0) {
            ELOG_DEBUG_EX(m_logger, "Address %p could not be matched with a loaded module",
                          address);
        } else {
            ELOG_DEBUG_EX(m_logger, "dladdr() returned: module %s at %p, sym name %s",
                          dlinfo.dli_fname, dlinfo.dli_fbase, dlinfo.dli_sname);
            moduleInfo->m_loadAddress = dlinfo.dli_fbase;
            if (dlinfo.dli_fname != nullptr) {
                moduleInfo->m_modulePath = dlinfo.dli_fname;
            }
        }
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxModuleManager::getCurrentProcessImagePath(std::string& path) {
    // this time we need to read into buffer and rely on null byte after program path
    // this is ugly, but there is no other way
    std::vector<char> buf;
    DbgUtilErr rc = OsUtil::readEntireFileToBuf("/proc/self/cmdline", buf);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    path = &buf[0];
    ELOG_DEBUG_EX(m_logger, "Current process image path is: %s", path.c_str());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxModuleManager::parseProcLine(std::string& line, std::string& imagePath,
                                             uint64_t& addrLo, uint64_t& addrHi) {
    // parse buffer to tokens
    std::istringstream iss(line);
    std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                    std::istream_iterator<std::string>{}};

    // line format is: <address-range> <mode> <offset> <id-pair> <inode-id> <file-path>
    // in some cases last token is missing
    if (tokens.size() == 5) {
        ELOG_DEBUG_EX(m_logger, "Skipping line with no module");
        return DBGUTIL_ERR_NOT_FOUND;
    }
    if (tokens.size() == 1) {
        ELOG_DEBUG_EX(m_logger, "Skipping line with one token");
        return DBGUTIL_ERR_NOT_FOUND;
    }
    if (tokens.size() != 6) {
        ELOG_DEBUG_EX(m_logger, "Line has invalid token count %u, aborting", tokens.size());
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    imagePath = tokens[5];
    std::string addrRange = tokens[0];
    std::string::size_type dashPos = addrRange.find('-');
    if (dashPos == std::string::npos) {
        ELOG_DEBUG_EX(m_logger, "Invalid address range: %s", addrRange.c_str());
        return DBGUTIL_ERR_DATA_CORRUPT;
    }
    try {
        std::size_t pos = 0;
        std::string addrPart1 = addrRange.substr(0, dashPos);
        addrLo = std::stoull(addrPart1, &pos, 16);
        if (pos != dashPos) {
            ELOG_DEBUG_EX(m_logger, "Invalid start of range: %s", addrPart1.c_str());
            return DBGUTIL_ERR_DATA_CORRUPT;
        }

        std::string addrPart2 = addrRange.substr(dashPos + 1);
        addrHi = std::stoull(addrPart2, &pos, 16);
        if (pos != (addrRange.length() - dashPos - 1)) {
            ELOG_DEBUG_EX(m_logger, "Invalid end of range: %s", addrPart2.c_str());
            return DBGUTIL_ERR_DATA_CORRUPT;
        }
    } catch (std::exception& e) {
        ELOG_SYS_ERROR_EX(m_logger, stoull, "Failed to parse address range %s: %s",
                          addrRange.c_str(), e.what());
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr initLinuxModuleManager() {
    LinuxModuleManager::createInstance();
    setModuleManager(LinuxModuleManager::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLinuxModuleManager() {
    setModuleManager(nullptr);
    LinuxModuleManager::destroyInstance();
    return DBGUTIL_ERR_OK;
}

#if 0
BEGIN_STARTUP_JOB(OsModuleManager) {
    LinuxModuleManager::createInstance();
    setModuleManager(LinuxModuleManager::getInstance());
    return DBGUTIL_ERR_OK;
}
END_STARTUP_JOB(OsModuleManager)

BEGIN_TEARDOWN_JOB(OsModuleManager) {
    setModuleManager(nullptr);
    LinuxModuleManager::destroyInstance();
    return DBGUTIL_ERR_OK;
}
END_TEARDOWN_JOB(OsModuleManager)
#endif

}  // namespace dbgutil

#endif  // DBGUTIL_LINUX