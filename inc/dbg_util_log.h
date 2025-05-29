#ifndef __DBGUTIL_LOG_H__
#define __DBGUTIL_LOG_H__

#include <cinttypes>

namespace dbgutil {

/** @enum Log severity constants */
enum LogSeverity {
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
class LogHandler {
public:
    /**
     * @brief Notifies that a logger has been registered.
     * @param severity The log severity with which the logger was initialized.
     * @param loggerName The name of the logger that was registered.
     * @param loggerId The identifier used to refer to this logger.
     * @return LogSeverity The desired severity for the logger. If not to be changed, then return
     * the severity with which the logger was registered.
     */
    virtual LogSeverity onRegisterLogger(LogSeverity severity, const char* loggerName,
                                         uint32_t loggerId) = 0;

    /**
     * @brief Notifies a logger is logging a message.
     * @param severity The log message severity.
     * @param loggerId The logger id.
     * @param msg The log message.
     */
    virtual void onMsg(LogSeverity severity, uint32_t loggerId, const char* msg) = 0;
};

}  // namespace dbgutil

#endif  // __DBGUTIL_LOG_H__
