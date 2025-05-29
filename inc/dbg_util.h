#ifndef __DBGUTIL_H__
#define __DBGUTIL_H__

#include "dbg_util_def.h"
#include "dbg_util_err.h"
#include "dbg_util_log.h"

namespace dbgutil {

/** @brief Initializes the debug utility library. */
extern DBGUTIL_API DbgUtilErr initDbgUtil(LogHandler* logHandler, LogSeverity severity);

/** @brief Terminates the debug utility library. */
extern DBGUTIL_API DbgUtilErr termDbgUtil();

/** @brief Configures global log severity */
extern DBGUTIL_API void setLogSeverity(LogSeverity severity);

/** @brief Configures log severity of a specific component. */
extern DBGUTIL_API void setComponentLogSeverity(const char* component, LogSeverity severity);

}  // namespace dbgutil

#endif  // __DBGUTIL_H__