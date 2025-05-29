#ifndef __DBGUTIL_LOG_IMP_H__
#define __DBGUTIL_LOG_IMP_H__

#include <cinttypes>
#include <string>

#include "dbg_util_def.h"
#include "dbg_util_log.h"

namespace dbgutil {

struct Logger {
    std::string m_loggerName;
    uint32_t m_loggerId;
    LogSeverity m_severity;
};

extern void setLogHandler(LogHandler* logHandler);

extern void setLogSeverity(LogSeverity severity);

extern void registerLogger(Logger& logger, const char* loggerName);

extern bool canLog(const Logger& logger, LogSeverity severity);

extern void logMsg(const Logger& logger, LogSeverity severity, const char* fmt, ...);

extern void startLog(const Logger& logger, LogSeverity severity, const char* fmt, ...);

extern void appendLog(const char* fmt, ...);

extern void appendLogNoFormat(const char* msg);

extern void finishLog();

extern const char* sysErrorToStr(int sysErrorCode);

#ifdef DBGUTIL_WINDOWS
extern char* win32SysErrorToStr(unsigned long sysErrorCode);
extern void win32FreeErrorStr(char* errStr);
#endif

}  // namespace dbgutil

// general logging macro
#define LOG(logger, severity, fmt, ...)               \
    if (canLog(logger, severity)) {                   \
        logMsg(logger, severity, fmt, ##__VA_ARGS__); \
    }

// per-severity logging macros
#define LOG_FATAL(logger, fmt, ...) LOG(logger, LS_FATAL, fmt, ##__VA_ARGS__)
#define LOG_ERROR(logger, fmt, ...) LOG(logger, LS_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(logger, fmt, ...) LOG(logger, LS_WARN, fmt, ##__VA_ARGS__)
#define LOG_NOTICE(logger, fmt, ...) LOG(logger, LS_NOTICE, fmt, ##__VA_ARGS__)
#define LOG_INFO(logger, fmt, ...) LOG(logger, LS_INFO, fmt, ##__VA_ARGS__)
#define LOG_TRACE(logger, fmt, ...) LOG(logger, LS_TRACE, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(logger, fmt, ...) LOG(logger, LS_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_DIAG(logger, fmt, ...) LOG(logger, LS_DIAG, fmt, ##__VA_ARGS__)

// multi-part logging macros
#define LOG_BEGIN(logger, severity, fmt, ...)           \
    if (canLog(logger, severity)) {                     \
        startLog(logger, severity, fmt, ##__VA_ARGS__); \
    }

#define LOG_APPEND(logger, fmt, ...) appendLog(logger, fmt, ##__VA_ARGS__)

#define LOG_APPEND_NF(logger, msg) appendLogNoFormat(logger, msg)

#define LOG_END(logger) finishLog(logger)

// system error logging macros
#define LOG_SYS_ERROR_NUM(logger, syscall, sysErr, fmt, ...)                \
    LOG_ERROR(logger, "System call " #syscall "() failed: %d (%s)", sysErr, \
              sysErrorToStr(sysErr));                                       \
    LOG_ERROR(logger, fmt, ##__VA_ARGS__);

#define LOG_SYS_ERROR(logger, syscall, fmt, ...) \
    LOG_SYS_ERROR_NUM(logger, syscall, errno, fmt, ##__VA_ARGS__)

// Windows system error logging macros
#ifdef DBGUTIL_WINDOWS
#define LOG_WIN32_ERROR_NUM(logger, syscall, sysErr, fmt, ...)                                   \
    {                                                                                            \
        char* errStr = win32SysErrorToStr(sysErr);                                               \
        LOG_ERROR(logger, "Windows system call " #syscall "() failed: %d (%s)", sysErr, errStr); \
        win32FreeErrorStr(errStr);                                                               \
        LOG_ERROR(logger, fmt, ##__VA_ARGS__);                                                   \
    }

#define LOG_WIN32_ERROR(logger, syscall, fmt, ...) \
    LOG_WIN32_ERROR_NUM(logger, syscall, ::GetLastError(), fmt, ##__VA_ARGS__)

#endif  // DBGUTIL_WINDOWS

#endif  // __DBGUTIL_LOG_IMP_H__