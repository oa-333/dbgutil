#ifndef __DBG_UTIL_H__
#define __DBG_UTIL_H__

#include "dbg_util_def.h"
#include "dbg_util_err.h"
#include "dbg_util_except.h"
#include "dbg_util_flags.h"
#include "dbg_util_log.h"

namespace dbgutil {

/**
 * @brief Initializes the debug utility library with internal logging enabled.
 *
 * @param exceptionListener Optional exception listener, that will receive notifications of any
 * fatal exceptions (i.e. access violation, segmentation fault, etc.).
 * @param logHandler Optional log handler to receive dbgutil internal log messages.
 * @param severity Optionally control the log severity of reported log messages. Any log messages
 * below this severity will be discarded. By default only fatal messages are sent to log.
 * @param flags Optional flags controlling the behavior of dbgutil. For a comprehensive list of
 * flags, please refer to the @ref dbg_util_flags.h header.
 * @return DBGUTIL_ERR_OK If succeeded, otherwise an error code.
 */
extern DBGUTIL_API DbgUtilErr initDbgUtil(OsExceptionListener* exceptionListener = nullptr,
                                          LogHandler* logHandler = nullptr,
                                          LogSeverity severity = LS_FATAL, uint32_t flags = 0);

/** @brief Terminates the debug utility library. */
extern DBGUTIL_API DbgUtilErr termDbgUtil();

/** @brief Queries whether the debug utility library is initialized. */
extern DBGUTIL_API bool isDbgUtilInitialized();

}  // namespace dbgutil

#endif  // __DBG_UTIL_H__