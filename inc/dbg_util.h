#ifndef __DBG_UTIL_H__
#define __DBG_UTIL_H__

#include "dbg_util_def.h"
#include "dbg_util_err.h"
#include "dbg_util_log.h"

namespace dbgutil {

/** @brief Initializes the debug utility library. */
extern DBGUTIL_API DbgUtilErr initDbgUtil(LogHandler* logHandler, LogSeverity severity);

/** @brief Terminates the debug utility library. */
extern DBGUTIL_API DbgUtilErr termDbgUtil();

}  // namespace dbgutil

#endif  // __DBG_UTIL_H__