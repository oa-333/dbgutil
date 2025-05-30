#include "dbg_util_def.h"

#ifdef DBGUTIL_GCC

#include "dwarf_util.h"
#include "linux_symbol_engine.h"
#include "os_image_reader.h"
#include "os_module_manager.h"
#include "os_symbol_engine.h"

#ifdef DBGUTIL_MINGW
// required for module info
#include "win32_symbol_engine.h"
#else
#include <dlfcn.h>
// #include <execinfo.h>
// #include <link.h>
#endif

#include <cxxabi.h>

#include <algorithm>
#include <cassert>
#include <vector>

#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"

namespace dbgutil {

// NOTE: since gcc/g++ uses the PE32 image differently than the MSFT linker, we cannot use
// directly Win32SymbolHandler. instead a more careful approach needs to take place (i.e. try to
// read PE32 image and see if there is a symbol table and GNU debug sections). This is unrelated to
// whether the application is running under MSYSTEM runtime environment or not. #ifdef DBGUTIL_MINGW

static Logger sLogger;

LinuxSymbolEngine* LinuxSymbolEngine::sInstance = nullptr;

void LinuxSymbolEngine::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) LinuxSymbolEngine();
    assert(sInstance != nullptr);
}

LinuxSymbolEngine* LinuxSymbolEngine::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void LinuxSymbolEngine::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

DbgUtilErr LinuxSymbolEngine::collectSymbolInfo(SymbolModuleData* symModData, void* symAddress,
                                                SymbolInfo& symbolInfo) {
    // get module details
    symbolInfo.m_moduleBaseAddress = symModData->m_moduleInfo.m_loadAddress;
    symbolInfo.m_moduleName = symModData->m_moduleInfo.m_modulePath;
    LOG_DEBUG(sLogger, "Symbol module image %s loaded at %p", symbolInfo.m_moduleName.c_str(),
              symbolInfo.m_moduleBaseAddress);

    // first search in binary image
    // this way we can also get start address of symbol and compute byte offset
    DbgUtilErr rc = symModData->m_imageReader->searchSymbol(
        symAddress, symbolInfo.m_symbolName, symbolInfo.m_fileName, &symbolInfo.m_startAddress);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_DEBUG(sLogger, "Failed to find symbol %p in binary image: %s", symAddress,
                  errorCodeToStr(rc));
    } else {
        symbolInfo.m_byteOffset = (uint64_t)symAddress - (uint64_t)symbolInfo.m_startAddress;
    }

    // next we go to dwarf data and try there
    rc = symModData->m_dwarfUtil.searchSymbol(
        symAddress, symbolInfo, (void*)symModData->m_imageReader->getRelocationBase());
    if (rc == DBGUTIL_ERR_OK) {
        LOG_DEBUG(sLogger, "Dwarf info: sym name %s, file %s, line %u",
                  symbolInfo.m_symbolName.c_str(), symbolInfo.m_fileName.c_str(),
                  symbolInfo.m_lineNumber);
    }

    // although we should know from image reader that the symbol table is empty (so we can
    // distinguish whether this is a Windows native DLL or a MinGW DLL built by gcc/g++), we just
    // give it a shot anyway if some detail is missing (logically, this will cover more edge cases)
    bool fromWin32SymHandler = false;
#ifdef DBGUTIL_MINGW
    if (rc != DBGUTIL_ERR_OK && rc == DBGUTIL_ERR_NOT_FOUND) {
        rc = Win32SymbolEngine::getInstance()->getSymbolInfo(symAddress, symbolInfo);
        if (rc == DBGUTIL_ERR_OK) {
            fromWin32SymHandler = true;
        }
    }
#endif

#ifdef DBGUTIL_LINUX
    if (symbolInfo.m_symbolName.empty() || symbolInfo.m_moduleName.empty() ||
        symbolInfo.m_moduleBaseAddress == nullptr) {
        Dl_info dlinfo;
        if (dladdr(symAddress, &dlinfo) == 0) {
            LOG_DEBUG(sLogger, "Symbol at %p could not be matched with a loaded module",
                      symAddress);
        } else {
            LOG_DEBUG(sLogger, "dladdr() returned: module %s at %p, sym name %s", dlinfo.dli_fname,
                      dlinfo.dli_fbase, dlinfo.dli_sname);
            if (symbolInfo.m_moduleName.empty()) {
                symbolInfo.m_moduleName = dlinfo.dli_fname;
            }
            if (symbolInfo.m_moduleBaseAddress == nullptr) {
                symbolInfo.m_moduleBaseAddress = dlinfo.dli_fbase;
            }

            // we might be already able to get the symbol name from dlinfo
            if (symbolInfo.m_symbolName.empty() && dlinfo.dli_sname != nullptr) {
                symbolInfo.m_symbolName = dlinfo.dli_sname;
            }
        }
    }
#endif

    // if we found a symbol name, then try to demangle it
    if (!symbolInfo.m_symbolName.empty() && !fromWin32SymHandler) {
        int status;
        char* demangledName =
            abi::__cxa_demangle(symbolInfo.m_symbolName.c_str(), nullptr, 0, &status);
        if (status == 0 && demangledName != nullptr) {
            symbolInfo.m_symbolName = demangledName;
        }
    }

    if (rc != DBGUTIL_ERR_OK) {
        LOG_DEBUG(sLogger, "Failed to get symbol %p info: %s", symAddress, errorCodeToStr(rc));
    }
    return rc;
}

SymbolModuleData* LinuxSymbolEngine::findSymbolModule(void* address) {
    // NOTE: assuming lock already taken!
    SymbolModuleData* symModData = nullptr;
    SymbolModuleSet::iterator itr = std::lower_bound(
        m_symbolModuleSet.begin(), m_symbolModuleSet.end(), address,
        [](SymbolModuleData* symModData, void* address) { return (*symModData) < address; });
    if (itr != m_symbolModuleSet.end() && (*itr)->contains(address)) {
        symModData = *itr;
    }
    return symModData;
}

void LinuxSymbolEngine::prepareModuleData(
    SymbolModuleData* symModData) {  // start with image reader
    symModData->m_imageReader = createImageReader();
    DbgUtilErr rc = symModData->m_imageReader->open(symModData->m_moduleInfo.m_modulePath.c_str(),
                                                    symModData->m_moduleInfo.m_loadAddress);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_DEBUG(sLogger, "Failed to open module image file %s for reading: %s",
                  symModData->m_moduleInfo.m_modulePath.c_str(), errorCodeToStr(rc));
        // image reader is kept open, we might still be able to use it
        // (file is already closed anyway, only relevant sections are kept in memory)
        // TODO: keep only required sections and drop entire file buffer
    }

    // now prepare dwarf stuff

    // collect debug section references from image reader
    symModData->m_imageReader->forEachSection(
        ".debug", [symModData, this](const OsImageSection& section) {
            LOG_DEBUG(sLogger, "Adding debug section: %s", section.m_name.c_str());
            symModData->m_dwarfData.addSection(section.m_name.c_str(),
                                               {section.m_start, section.m_size});
            return true;  // continue traversing sections
        });

    // if all sections are present then parse initial dwarf data
    if (!symModData->m_dwarfData.checkDebugSections()) {
        LOG_DEBUG(sLogger, "Not all required debug sections found, skipping by dwarf");
    } else {
        rc = symModData->m_dwarfUtil.open(
            symModData->m_dwarfData, symModData->m_moduleInfo.m_loadAddress,
            symModData->m_imageReader->getIs64Bit(), symModData->m_imageReader->getIsExe());
        if (rc != DBGUTIL_ERR_OK) {
            LOG_DEBUG(sLogger, "Failed to open dwarf data: %s", errorCodeToStr(rc));
        } else {
            symModData->m_dwarfUtilValid = true;
        }
    }
}

DbgUtilErr LinuxSymbolEngine::getSymbolInfo(void* symAddress, SymbolInfo& symbolInfo) {
    // search module with read-lock
    SymbolModuleData* symModData = nullptr;
    {
        std::shared_lock<std::shared_mutex> lock(m_lock);
        symModData = findSymbolModule(symAddress);
    }

    // if the module exists then collect symbol info from there
    // NOTE: we don't need global read-lock as the module object never changes nor gets deleted
    if (symModData != nullptr) {
        // must with for module to be ready for use
        symModData->waitReady();
        return collectSymbolInfo(symModData, symAddress, symbolInfo);
    }

    // module was not found, so we need to get the module info, setup an image reader, dwarf reader,
    // etc. in order to avoid possible race, only one can insert and the rest wait until the object
    // is ready.

    // first search in all system modules if address is relevant
    LOG_DEBUG(sLogger, "Searching for symbol %p", symAddress);
    OsModuleInfo moduleInfo;
    DbgUtilErr rc = getModuleManager()->getModuleByAddress(symAddress, moduleInfo);
    if (rc != DBGUTIL_ERR_OK || moduleInfo.m_loadAddress == nullptr) {
        LOG_DEBUG(sLogger, "Failed to find module for symbol %p: %s", symAddress,
                  errorCodeToStr(rc));
        return rc;
    }

    // insert an entry under write lock
    bool shouldWait = false;
    {
        std::unique_lock<std::shared_mutex> lock(m_lock);
        // handle race, in case another thread is doing the same thing
        symModData = findSymbolModule(symAddress);
        if (symModData != nullptr) {
            // just wait for the first thread to finish preparing the module data
            // but we do this wait outside lock scope
            shouldWait = true;
        } else {
            // insert a half-baked module (no race, we are under write lock)
            symModData = new (std::nothrow) SymbolModuleData();
            symModData->m_moduleInfo = moduleInfo;
            m_symbolModuleSet.push_back(symModData);
            std::sort(m_symbolModuleSet.begin(), m_symbolModuleSet.end(),
                      SymbolModuleDataCompare());
            // note entry is not ready yet, but other module data can be accessed while we prepare
            // the module info object
        }
    }

    if (shouldWait) {
        symModData->waitReady();
    } else {
        prepareModuleData(symModData);
        symModData->setReady();
    }

    // now all threads can collect symbol data concurrently
    return collectSymbolInfo(symModData, symAddress, symbolInfo);
}

LinuxSymbolEngine::LinuxSymbolEngine() {}

DbgUtilErr initLinuxSymbolEngine() {
    registerLogger(sLogger, "linux_symbol_engine");
    LinuxSymbolEngine::createInstance();
    setSymbolEngine(LinuxSymbolEngine::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLinuxSymbolEngine() {
    setSymbolEngine(nullptr);
    LinuxSymbolEngine::destroyInstance();
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

#if 0
BEGIN_STARTUP_JOB(LinuxSymbolEngine) {
    LinuxSymbolEngine::createInstance();
#ifdef DBGUTIL_GCC
    setSymbolEngine(LinuxSymbolEngine::getInstance());
#endif
    return DBGUTIL_ERR_OK;
}
END_STARTUP_JOB(LinuxSymbolEngine)
DECLARE_STARTUP_JOB_DEP(LinuxSymbolEngine, OsModuleManager)

BEGIN_TEARDOWN_JOB(LinuxSymbolEngine) {
#ifdef DBGUTIL_MSVC
    setSymbolEngine(nullptr);
#endif
    LinuxSymbolEngine::destroyInstance();
    return DBGUTIL_ERR_OK;
}
END_TEARDOWN_JOB(LinuxSymbolEngine)
#endif

}  // namespace dbgutil

#endif