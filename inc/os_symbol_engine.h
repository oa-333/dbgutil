#ifndef __OS_SYMBOL_ENGINE_H__
#define __OS_SYMBOL_ENGINE_H__

#include <cinttypes>
#include <string>

#include "dbg_util_def.h"
#include "dbg_util_err.h"

namespace dbgutil {

/** @brief Symbol infomation. */
struct SymbolInfo {
    /** @brief The containing module's base address in memory. */
    void* m_moduleBaseAddress;

    /** @brief The start address of the symbol. */
    void* m_startAddress;

    /** @brief The bytes offset of the symbol address from the start of the symbol. */
    uint32_t m_byteOffset;

    /** @brief The line number of the symbol. */
    uint32_t m_lineNumber;

    /** @brief The column index of the symbol (Dwarf only). */
    uint32_t m_columnIndex;

    /** @brief Alignment. */
    uint32_t m_padding;

    /** @brief The name of the symbol. */
    std::string m_symbolName;

    /** @brief The name of the file containing the symbol. */
    std::string m_fileName;

    /** @brief The name of the module containing the symbol. */
    std::string m_moduleName;

    SymbolInfo()
        : m_moduleBaseAddress(nullptr),
          m_startAddress(nullptr),
          m_byteOffset(0),
          m_lineNumber(0),
          m_columnIndex(0) {}
};

/** @brief Parent interface for symbol engines. */
class DBGUTIL_API OsSymbolEngine {
public:
    OsSymbolEngine(const OsSymbolEngine&) = delete;
    OsSymbolEngine(OsSymbolEngine&&) = delete;
    virtual ~OsSymbolEngine() {}

    /**
     * @brief Retrieves symbol debug information (platform independent API).
     * @param symAddress The symbol address.
     * @param[out] symbolInfo The symbol information.
     */
    virtual DbgUtilErr getSymbolInfo(void* symAddress, SymbolInfo& symbolInfo) = 0;

protected:
    OsSymbolEngine() {}
};

/** @brief Installs a symbol engine implementation. */
extern DBGUTIL_API void setSymbolEngine(OsSymbolEngine* symbolEngine);

/** @brief Retrieves the installed symbol engine implementation. */
extern DBGUTIL_API OsSymbolEngine* getSymbolEngine();

}  // namespace dbgutil

#endif  // __OS_SYMBOL_ENGINE_H__