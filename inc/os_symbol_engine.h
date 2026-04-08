#ifndef __OS_SYMBOL_ENGINE_H__
#define __OS_SYMBOL_ENGINE_H__

#include <cinttypes>
#include <list>
#include <string>

#include "dbg_util_def.h"
#include "dbg_util_err.h"

namespace dbgutil {

/** @brief Symbol infomation. */
struct DBGUTIL_API SymbolInfo {
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

    /** @brief Size of symbol in bytes. */
    uint32_t m_symbolSize;

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

    SymbolInfo(const SymbolInfo&) = default;
    SymbolInfo(SymbolInfo&&) = default;
    SymbolInfo& operator=(const SymbolInfo&) = default;
    ~SymbolInfo() {}

    void merge(const SymbolInfo& symInfo) {
        if (m_moduleBaseAddress == nullptr) {
            m_moduleBaseAddress = symInfo.m_moduleBaseAddress;
        }
        if (m_startAddress == nullptr) {
            m_startAddress = symInfo.m_startAddress;
        }
        if (m_byteOffset == 0) {
            m_byteOffset = symInfo.m_byteOffset;
        }
        if (m_lineNumber == 0) {
            m_lineNumber = symInfo.m_lineNumber;
        }
        if (m_columnIndex == 0) {
            m_columnIndex = symInfo.m_columnIndex;
        }
        if (m_symbolName.empty()) {
            m_symbolName = symInfo.m_symbolName;
        }
        if (m_fileName.empty()) {
            m_fileName = symInfo.m_fileName;
        }
        if (m_moduleName.empty()) {
            m_moduleName = symInfo.m_moduleName;
        }
    }
};

/** @class Symbol info visitor for traversing symbols. */
class DBGUTIL_API SymbolInfoVisitor {
public:
    virtual ~SymbolInfoVisitor() {}

    /**
     * @brief Visits a symbol.
     *
     * @param symbolInfo The symbol information.
     * @param[out] shouldStop Set this to tru in order to stop traversing symbols.
     * @return DbgUtilErr The visit operation result.
     */
    virtual DbgUtilErr onSymbolInfo(const SymbolInfo& symbolInfo, bool& shouldStop) = 0;

protected:
    SymbolInfoVisitor() {}
    SymbolInfoVisitor(const SymbolInfoVisitor&) = delete;
    SymbolInfoVisitor(SymbolInfoVisitor&&) = delete;
    SymbolInfoVisitor& operator=(const SymbolInfoVisitor&) = delete;
};

/** @brief Parent interface for symbol engines. */
class DBGUTIL_API OsSymbolEngine {
public:
    OsSymbolEngine(const OsSymbolEngine&) = delete;
    OsSymbolEngine(OsSymbolEngine&&) = delete;
    OsSymbolEngine& operator=(const OsSymbolEngine&) = delete;
    virtual ~OsSymbolEngine() {}

    /**
     * @brief Retrieves symbol debug information by symbol address (platform independent API).
     * @param symAddress The symbol address.
     * @param[out] symbolInfo The symbol information.
     * @return Operation's result.
     */
    virtual DbgUtilErr getSymbolInfo(void* symAddress, SymbolInfo& symbolInfo) = 0;

    /**
     * @brief Retrieves symbol debug information by symbol name (platform independent API).
     * @param symbolName The symbol name to search (exact match).
     * @param moduleNameRegex Regular expression for limiting the searched modules.
     * @param[out] symbolInfo The symbol information.
     * @return Operation's result.
     *
     * @note Unless the module regular expression is specified, this call will may cause all modules
     * to be loaded.
     */
    virtual DbgUtilErr getSymbolInfo(const char* symbolName, const char* moduleNameRegex,
                                     SymbolInfo& symbolInfo);

    /**
     * @brief Searches for symbol debug information by symbol name regular expression (platform
     * independent API).
     * @param symbolRegex The symbol name regular expression.
     * @param moduleNameRegex Regular expression for limiting the searched modules.
     * @param[out] symbolInfoList The resulting list of matching symbol information.
     * @param maxSymbolCount Optional limit on number of modules to return.
     * @return Operation's result.
     *
     * @note Unless the module regular expression is specified, this call will cause all modules to
     * be loaded. Also if the expected result list size is too big, consider using @ref visitSymbols
     * instead.
     */
    virtual DbgUtilErr searchSymbols(const char* symbolRegex, const char* moduleNameRegex,
                                     std::list<SymbolInfo>& symbolInfoList,
                                     size_t maxSymbolCount = SIZE_MAX);

    /**
     * @brief Traverses all symbols having a name that matches a regular expression. This variant
     * can be used if @ref searchSymbols() may yield too many symbols at once.
     * @param visitor The thread visitor.
     * @return The operation result.
     */
    virtual DbgUtilErr visitSymbols(const char* symbolRegex, const char* moduleNameRegex,
                                    SymbolInfoVisitor* visitor) = 0;

protected:
    OsSymbolEngine() {}
};

/** @brief Retrieves the installed symbol engine implementation. */
extern DBGUTIL_API OsSymbolEngine* getSymbolEngine();

/** @brief Utility API for lambda syntax. */
template <typename F>
inline DbgUtilErr forEachSymbol(const char* symbolRegex, const char* moduleNameRegex, F f) {
    struct Visitor final : public SymbolInfoVisitor {
        Visitor(F f) : m_f(f) {}
        Visitor() = delete;
        Visitor(const Visitor&) = delete;
        Visitor(Visitor&&) = delete;
        Visitor& operator=(const Visitor&) = delete;
        ~Visitor() final {}
        DbgUtilErr onSymbolInfo(const SymbolInfo& symbolInfo, bool& shouldStop) {
            return m_f(symbolInfo, shouldStop);
        }
        F m_f;
    };
    Visitor visitor(f);
    return getSymbolEngine()->visitSymbols(symbolRegex, moduleNameRegex, &visitor);
}

}  // namespace dbgutil

#endif  // __OS_SYMBOL_ENGINE_H__