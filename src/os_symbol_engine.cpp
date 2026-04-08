#include "os_symbol_engine.h"

#include <cassert>

#include "os_symbol_engine_internal.h"

namespace dbgutil {

static OsSymbolEngine* sSymbolEngine = nullptr;

DbgUtilErr OsSymbolEngine::getSymbolInfo(const char* symbolName, const char* moduleNameRegex,
                                         SymbolInfo& symbolInfo) {
    std::list<SymbolInfo> symbolInfoList;
    DbgUtilErr rc = searchSymbols(symbolName, moduleNameRegex, symbolInfoList, 1);
    if (rc == DBGUTIL_ERR_OK) {
        if (symbolInfoList.empty()) {
            rc = DBGUTIL_ERR_NOT_FOUND;
        } else if (symbolInfoList.size() > 1) {
            rc = DBGUTIL_ERR_INTERNAL_ERROR;
        } else {
            symbolInfo = symbolInfoList.back();
        }
    }
    return rc;
}

DbgUtilErr OsSymbolEngine::searchSymbols(const char* symbolRegex, const char* moduleNameRegex,
                                         std::list<SymbolInfo>& symbolInfoList,
                                         size_t maxSymbolCount /* = SIZE_MAX */) {
    // use convenient lambda syntax
    return forEachSymbol(
        symbolRegex, moduleNameRegex,
        [&symbolInfoList, maxSymbolCount](const SymbolInfo& symbolInfo,
                                          bool& shouldStop) -> DbgUtilErr {
            symbolInfoList.push_back(symbolInfo);
            if (maxSymbolCount != SIZE_MAX && symbolInfoList.size() == maxSymbolCount) {
                shouldStop = true;
            }
            return DBGUTIL_ERR_OK;
        });
}

void setSymbolEngine(OsSymbolEngine* symbolEngine) {
    assert((symbolEngine != nullptr && sSymbolEngine == nullptr) ||
           (symbolEngine == nullptr && sSymbolEngine != nullptr));
    sSymbolEngine = symbolEngine;
}

OsSymbolEngine* getSymbolEngine() {
    assert(sSymbolEngine != nullptr);
    return sSymbolEngine;
}

}  // namespace dbgutil
