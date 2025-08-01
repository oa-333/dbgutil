#ifndef __LINUX_SYMBOL_ENGINE_H__
#define __LINUX_SYMBOL_ENGINE_H__

#include "libdbg_def.h"

#ifdef LIBDBG_GCC

#include <condition_variable>
#include <mutex>
#include <shared_mutex>

#include "dwarf_util.h"
#include "os_image_reader.h"
#include "os_module_manager.h"
#include "os_symbol_engine.h"

namespace libdbg {

// cached module data for symbol search
struct SymbolModuleData {
    OsModuleInfo m_moduleInfo;
    OsImageReader* m_imageReader;
    DwarfData m_dwarfData;
    DwarfUtil m_dwarfUtil;
    bool m_dwarfUtilValid;
    std::mutex m_lock;
    std::condition_variable m_cv;
    bool m_isReady;

    SymbolModuleData() : m_imageReader(nullptr), m_dwarfUtilValid(false), m_isReady(false) {}

    inline void setReady() {
        std::unique_lock<std::mutex> lock(m_lock);
        m_isReady = true;
        m_cv.notify_all();
    }

    inline void waitReady() {
        std::unique_lock<std::mutex> lock(m_lock);
        m_cv.wait(lock, [this]() { return m_isReady; });
    }

    inline bool operator<(const SymbolModuleData& symModData) const {
        return m_moduleInfo < symModData.m_moduleInfo;
    }

    inline bool operator<(void* address) const { return m_moduleInfo < address; }

    inline bool contains(void* address) const { return m_moduleInfo.contains(address); }
};

struct SymbolModuleDataCompare {
    inline bool operator()(const SymbolModuleData* lhs, const SymbolModuleData* rhs) const {
        return (*lhs) < (*rhs);
    }
    inline bool operator()(const SymbolModuleData* symModData, void* address) const {
        return (*symModData) < address;
    }
};

class LinuxSymbolEngine : public OsSymbolEngine {
public:
    /** @brief Creates the singleton instance of the module manager for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the module manager. */
    static LinuxSymbolEngine* getInstance();

    /** @brief Destroys the singleton instance of the module manager. */
    static void destroyInstance();

    /**
     * @brief Retrieves symbol debug information (platform independent API).
     * @param symAddress The symbol address.
     * @param[out] symbolInfo The symbol information.
     */
    LibDbgErr getSymbolInfo(void* symAddress, SymbolInfo& symbolInfo) final;

private:
    LinuxSymbolEngine();
    ~LinuxSymbolEngine() {}

    static LinuxSymbolEngine* sInstance;

    typedef std::vector<SymbolModuleData*> SymbolModuleSet;
    SymbolModuleSet m_symbolModuleSet;
    std::shared_mutex m_lock;

    LibDbgErr collectSymbolInfo(SymbolModuleData* symModData, void* symAddress,
                                SymbolInfo& symbolInfo);

    SymbolModuleData* findSymbolModule(void* address);
    void prepareModuleData(SymbolModuleData* symModData);
};

extern LibDbgErr initLinuxSymbolEngine();
extern LibDbgErr termLinuxSymbolEngine();

}  // namespace libdbg

#endif  // LIBDBG_GCC

#endif  // __LINUX_SYMBOL_ENGINE_H__