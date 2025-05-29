#include "dbg_util_def.h"

#ifdef DBGUTIL_MINGW
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <vector>

#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "log_msg_builder.h"

#define LOG_BUFFER_SIZE 1024

namespace dbgutil {

struct LogData {
    const Logger* m_logger;
    LogSeverity m_severity;
    LogMsgBuilder m_msgBuilder;

    LogData() { reset(); }

    inline void reset(const Logger* logger = nullptr, LogSeverity severity = LS_INFO) {
        m_logger = logger;
        m_severity = severity;
        m_msgBuilder.reset();
    }
};

static thread_local LogData sLogData;

static LogHandler* sLogHandler = nullptr;
static LogSeverity sLogSeverity = LS_INFO;
static std::vector<Logger*> sLoggers;

/** @brief Queries whether a multi-part log message is being constructed. */
static bool isLogging() { return sLogData.m_msgBuilder.getOffset() > 0; }

static void appendMsgV(const char* fmt, va_list ap) {
    va_list apCopy;
    va_copy(apCopy, ap);
    uint32_t requiredBytes = (vsnprintf(nullptr, 0, fmt, apCopy) + 1);
    if (sLogData.m_msgBuilder.ensureBufferLength(requiredBytes)) {
        sLogData.m_msgBuilder.appendV(fmt, ap);
    }
    va_end(apCopy);
}

static void appendMsg(const char* msg) {
    uint32_t requiredBytes = (strlen(msg) + 1);
    if (sLogData.m_msgBuilder.ensureBufferLength(requiredBytes)) {
        sLogData.m_msgBuilder.append(msg);
    }
}

void setLogHandler(LogHandler* logHandler) { sLogHandler = logHandler; }

void setLogSeverity(LogSeverity severity) { sLogSeverity = severity; }

void registerLogger(Logger& logger, const char* loggerName) {
    logger.m_loggerId = sLoggers.size();
    sLoggers.push_back(&logger);
    logger.m_loggerName = loggerName;
    logger.m_severity = sLogHandler->onRegisterLogger(sLogSeverity, loggerName, logger.m_loggerId);
}

bool canLog(const Logger& logger, LogSeverity severity) {
    return severity >= sLogSeverity && severity >= logger.m_severity;
}

void logMsg(const Logger& logger, LogSeverity severity, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (!isLogging()) {
        sLogData.reset(&logger, severity);
        appendMsgV(fmt, ap);
        finishLog();
    } else {
        fprintf(stderr,
                "Attempt to log message while previous start-log call has not finished yet: ");
        vfprintf(stderr, fmt, ap);
        fputs("\n", stderr);
        fflush(stderr);
    }
    va_end(ap);
}

void startLog(const Logger& logger, LogSeverity severity, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (!isLogging()) {
        sLogData.reset(&logger, severity);
        appendMsgV(fmt, ap);
    } else {
        fprintf(
            stderr,
            "Attempt to start log message while previous start-log call has not finished yet: ");
        vfprintf(stderr, fmt, ap);
        fputs("\n", stderr);
        fflush(stderr);
    }
    va_end(ap);
}

void appendLog(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (isLogging()) {
        appendMsgV(fmt, ap);
    } else {
        fprintf(stderr, "Attempt to append log message without start-log being issued first: ");
        vfprintf(stderr, fmt, ap);
        fputs("\n", stderr);
        fflush(stderr);
    }
    va_end(ap);
}

void appendLogNoFormat(const char* msg) {
    if (isLogging()) {
        appendMsg(msg);
    } else {
        fprintf(stderr,
                "Attempt to append unformatted log message without start-log being issued first: ");
        fputs(msg, stderr);
        fputs("\n", stderr);
        fflush(stderr);
    }
}

void finishLog() {
    if (isLogging()) {
        // NOTE: new line character at the end of the line is added by log handler if at all
        const char* logMsg = sLogData.m_msgBuilder.finalize();
        sLogHandler->onMsg(sLogData.m_severity, sLogData.m_logger->m_loggerId, logMsg);
        sLogData.reset();
    } else {
        fprintf(stderr, "attempt to end log message without start-log being issued first\n");
    }
}

const char* sysErrorToStr(int sysErrorCode) {
    const int BUF_LEN = 256;
    static thread_local char buf[BUF_LEN];
#ifdef DBGUTIL_WINDOWS
    (void)strerror_s(buf, BUF_LEN, sysErrorCode);
    return buf;
#else
#if (_POSIX_C_SOURCE >= 200112L) && !_GNU_SOURCE
    (void)strerror_r(sysErrorCode, buf, BUF_LEN);
    return buf;
#else
    return strerror_r(sysErrorCode, buf, BUF_LEN);
#endif
#endif
}

#ifdef DBGUTIL_WINDOWS
char* win32SysErrorToStr(unsigned long sysErrorCode) {
    LPSTR messageBuffer = nullptr;
    std::size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, sysErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0,
        NULL);
    return messageBuffer;
}

void win32FreeErrorStr(char* errStr) { LocalFree(errStr); }

#endif

}  // namespace dbgutil
