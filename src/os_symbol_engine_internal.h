#ifndef __OS_SYMBOL_ENGINE_INTERNAL_H__
#define __OS_SYMBOL_ENGINE_INTERNAL_H__

#include "os_symbol_engine.h"

namespace dbgutil {

/** @brief Installs a symbol engine implementation. */
extern void setSymbolEngine(OsSymbolEngine* symbolEngine);

}  // namespace dbgutil

#endif  // __OS_SYMBOL_ENGINE_INTERNAL_H__