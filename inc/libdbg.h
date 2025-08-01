#ifndef __LIBDBG_H__
#define __LIBDBG_H__

#include "libdbg_def.h"
#include "libdbg_err.h"
#include "libdbg_except.h"
#include "libdbg_flags.h"
#include "libdbg_log.h"

namespace libdbg {

/**
 * @brief Initializes the debug utility library with internal logging enabled.
 *
 * @param exceptionListener Optional exception listener, that will receive notifications of any
 * fatal exceptions (i.e. access violation, segmentation fault, etc.).
 * @param logHandler Optional log handler to receive dbgutil internal log messages.
 * @param severity Optionally control the log severity of reported log messages. Any log messages
 * below this severity will be discarded. By default only fatal messages are sent to log.
 * @param flags Optional flags controlling the behavior of dbgutil. For a comprehensive list of
 * flags, please refer to the @ref libdbg_flags.h header.
 * @return LIBDBG_ERR_OK If succeeded, otherwise an error code.
 */
extern LIBDBG_API LibDbgErr initLibDbg(OsExceptionListener* exceptionListener = nullptr,
                                       LogHandler* logHandler = nullptr,
                                       LogSeverity severity = LS_FATAL, uint32_t flags = 0);

/** @brief Terminates the debug utility library. */
extern LIBDBG_API LibDbgErr termLibDbg();

}  // namespace libdbg

#endif  // __LIBDBG_H__