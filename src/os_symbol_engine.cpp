#include "os_symbol_engine.h"

#include <cassert>

#include "os_symbol_engine_internal.h"

namespace dbgutil {

static OsSymbolEngine* sSymbolEngine = nullptr;

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
