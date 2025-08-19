#ifndef __DBG_UTIL_LOG_H__
#define __DBG_UTIL_LOG_H__

#include <cinttypes>
#include <cstddef>

#include "dbg_util_def.h"

namespace dbgutil {

/** @enum Log severity constants */
enum LogSeverity : uint32_t {
    /** @brief Fatal severity */
    LS_FATAL,

    /** @brief Error severity */
    LS_ERROR,

    /** @brief Warning severity */
    LS_WARN,

    /** @brief Notice severity */
    LS_NOTICE,

    /** @brief Informative severity */
    LS_INFO,

    /** @brief Trace severity */
    LS_TRACE,

    /** @brief Debug severity */
    LS_DEBUG,

    /** @brief Diagnostics severity */
    LS_DIAG
};

/** @class Log handler for handling log messages coming from the Debug Utilities library. */
class DBGUTIL_API LogHandler {
public:
    /** @brief Virtual destructor. */
    virtual ~LogHandler() {}

    /**
     * @brief Notifies that a logger has been registered.
     * @param severity The log severity with which the logger was initialized.
     * @param loggerName The name of the logger that was registered.
     * @param loggerId The identifier used to refer to this logger.
     * @return LogSeverity The desired severity for the logger. If not to be changed, then return
     * the severity with which the logger was registered.
     */
    virtual LogSeverity onRegisterLogger(LogSeverity severity, const char* /* loggerName */,
                                         size_t /* loggerId */) {
        return severity;
    }

    /** @brief Unregisters a previously registered logger. */
    virtual void onUnregisterLogger(size_t /* loggerId */) {}

    /**
     * @brief Notifies a logger is logging a message.
     * @param severity The log message severity.
     * @param loggerId The logger's id.
     * @param loggerName The logger's name.
     * @param msg The log message.
     */
    virtual void onMsg(LogSeverity severity, size_t loggerId, const char* loggerName,
                       const char* msg) = 0;

protected:
    LogHandler() {}
    LogHandler(const LogHandler&) = delete;
    LogHandler(LogHandler&&) = delete;
    LogHandler& operator=(const LogHandler&) = delete;
};

/** @brief Configures global log severity */
extern DBGUTIL_API void setLogSeverity(LogSeverity severity);

/** @brief Configures log severity of a specific logger. */
extern DBGUTIL_API void setLoggerSeverity(size_t loggerId, LogSeverity severity);

/** @brief Converts log severity to string. */
extern DBGUTIL_API const char* logSeverityToString(LogSeverity severity);

/** @def A special constant denoting default log handler (prints to standard error stream). */
#define DBGUTIL_DEFAULT_LOG_HANDLER ((dbgutil::LogHandler*)-1)

}  // namespace dbgutil

#endif  // __DBG_UTIL_LOG_H__
