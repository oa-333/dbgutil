#define DBGUTIL_NO_WINDOWS_HEADER
#include "libdbg_def.h"

// it turns out that when running from within MinGW/UCRT console, then signal handlers can be
// registered, but when running from Windows console Windows exception handler is in effect.
// this can be distinguished by the existence of the MSYSTEM environment variable
#ifdef DBGUTIL_GCC

#include <signal.h>
#include <string.h>

#include <cassert>
#include <cinttypes>

#include "dbg_stack_trace.h"
#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "libdbg_flags.h"
#include "linux_exception_handler.h"
#include "linux_stack_trace.h"

namespace libdbg {

static Logger sLogger;

// thread local static buffer for exception information (avoid allocation during panic)
#define EXCEPTION_BUF_SIZE 256
static thread_local char sExceptBuf[EXCEPTION_BUF_SIZE];
static int sExceptBufLen = 0;

LinuxExceptionHandler* LinuxExceptionHandler::sInstance = nullptr;

#ifdef DBGUTIL_LINUX
static const char* getSigIllInfo(int code) {
    switch (code) {
        case ILL_ILLOPC:
            return "Illegal opcode";

        case ILL_ILLOPN:
            return "Illegal operand";

        case ILL_ILLADR:
            return "Illegal addressing mode";

        case ILL_PRVOPC:
            return "Privileged opcode";

        case ILL_COPROC:
            return "Coprocessor error";

        case ILL_BADSTK:
            return "Internal stack error";

        default:
            return "N/A";
    }
}

static const char* getSigFpeInfo(int code) {
    switch (code) {
        case FPE_INTDIV:
            return "Integer division by zero";

        case FPE_INTOVF:
            return "Integer overflow";

        case FPE_FLTDIV:
            return "Floating-point divide by zero";

        case FPE_FLTOVF:
            return "Floating-point overflow";

        case FPE_FLTUND:
            return "Floating-point underflow";

        case FPE_FLTRES:
            return "Floating-point inexact result";

        case FPE_FLTINV:
            return "Floating-point invalid operation";

        case FPE_FLTSUB:
            return "Subscript out of range";

        default:
            return "N/A";
    }
}

static const char* getSigSegvInfo(int code) {
    switch (code) {
        case SEGV_MAPERR:
            return "Address not mapped to object";

        case SEGV_ACCERR:
            return "Invalid permissions for mapped object";

        default:
            return "N/A";
    }
}

static const char* getSigBusInfo(int code) {
    switch (code) {
        case BUS_ADRALN:
            return "Invalid address alignment";

        case BUS_ADRERR:
            return "Nonexistent physical address";

        case BUS_OBJERR:
            return "Object - specific hardware error";

        case BUS_MCEERR_AR:
            return "Hardware memory error consumed on a machine check";

        default:
            return "N/A";
    }
}

static const char* getSigInfo(int sigNum, int code) {
    switch (sigNum) {
        case SIGILL:
            return getSigIllInfo(code);

        case SIGFPE:
            return getSigFpeInfo(code);

        case SIGSEGV:
            return getSigSegvInfo(code);

        case SIGBUS:
            return getSigBusInfo(code);

        default:
            return "N/A";
    }
}

static void printExtendedInfo(int sigNum, int code) {
    sExceptBufLen += snprintf(sExceptBuf + sExceptBufLen, EXCEPTION_BUF_SIZE - sExceptBufLen,
                              "Extended exception information: %s\n", getSigInfo(sigNum, code));
}
#endif

#ifdef DBGUTIL_MINGW
static const char* mingwGetSignalName(int sigNum) {
    switch (sigNum) {
        case SIGSEGV:
            return "Segmentation fault";

        case SIGILL:
            return "Illegal address or operand";

        case SIGFPE:
            return "Floating point exception";

        default:
            return "N/A";
    }
}

void LinuxExceptionHandler::signalHandlerStatic(int sigNum) {
    LinuxExceptionHandler* exceptionHandler = (LinuxExceptionHandler*)getExceptionHandler();
    exceptionHandler->signalHandler(sigNum);
}

void LinuxExceptionHandler::signalHandler(int sigNum) {
    // prepare exception information
    OsExceptionInfo exInfo;
    exInfo.m_exceptionCode = sigNum;
    exInfo.m_exceptionSubCode = 0;
    exInfo.m_exceptionName = getSignalName(sigNum);
    exInfo.m_faultAddress = nullptr;  // MinGW has no fault address

    sExceptBuf[0] = 0;
    sExceptBufLen = snprintf(sExceptBuf, EXCEPTION_BUF_SIZE, "Received signal %d: %s\n", sigNum,
                             exInfo.m_exceptionName);

    // no fault address
    // no extended info

    // do platform-agnostic stuff
    finalizeSignalHandling(exInfo, nullptr);
}
#else
void LinuxExceptionHandler::signalHandlerStatic(int sigNum, siginfo_t* sigInfo, void* context) {
    LinuxExceptionHandler* exceptionHandler = (LinuxExceptionHandler*)getExceptionHandler();
    exceptionHandler->signalHandler(sigNum, sigInfo, context);
}

void LinuxExceptionHandler::signalHandler(int sigNum, siginfo_t* sigInfo, void* context) {
    // prepare exception information
    OsExceptionInfo exInfo;
    exInfo.m_exceptionCode = sigNum;
    exInfo.m_exceptionSubCode = sigInfo->si_code;
    exInfo.m_exceptionName = getSignalName(sigNum);
    exInfo.m_faultAddress = sigInfo->si_addr;

    sExceptBuf[0] = 0;
    sExceptBufLen = snprintf(sExceptBuf, EXCEPTION_BUF_SIZE, "Received signal %d: %s\n", sigNum,
                             exInfo.m_exceptionName);

    // printf fault address
    sExceptBufLen += snprintf(sExceptBuf + sExceptBufLen, EXCEPTION_BUF_SIZE - sExceptBufLen,
                              "Faulting address: %p\n", exInfo.m_faultAddress);

    // print extended information
    printExtendedInfo(sigNum, sigInfo->si_code);

    // do platform-agnostic stuff
    finalizeSignalHandling(exInfo, context);
}
#endif

void LinuxExceptionHandler::finalizeSignalHandling(OsExceptionInfo& exInfo, void* context) {
    // ensure full-info member is set
    exInfo.m_fullExceptionInfo = sExceptBuf;

    // get stack trace information
    // NOTE: on Linux, using the context record results in one missing frame, so instead we pass
    // nullptr and let libunwind get full stack trace from this point
    exInfo.m_callStack = prepareCallStack(nullptr);

    // now we can dispatch the exception
    dispatchExceptionInfo(exInfo);

    // nevertheless, we also send to log
    if (getGlobalFlags() & LIBDBG_LOG_EXCEPTIONS) {
        LOG_FATAL(sLogger, exInfo.m_fullExceptionInfo);
        LOG_FATAL(sLogger, exInfo.m_callStack);
    }

    // generate core
    if (getGlobalFlags() && LIBDBG_EXCEPTION_DUMP_CORE) {
        LOG_FATAL(sLogger, "Aborting after fatal exception, see details above.");
        abort();
    }
}

const char* LinuxExceptionHandler::getSignalName(int sigNum) {
#ifdef DBGUTIL_MINGW
    return mingwGetSignalName(sigNum);
#else
    return strsignal(sigNum);
#endif
}

LibDbgErr LinuxExceptionHandler::registerSignalHandler(int sigNum, SignalHandlerFunc handler,
                                                       SignalHandler* prevHandler) {
#ifdef DBGUTIL_MINGW
    SignalHandler prevHandlerLocal = signal(sigNum, handler);
    if (prevHandlerLocal == SIG_ERR) {
        LOG_SYS_ERROR(sLogger, signal, "Failed to register signal handler for signal %d (%s)",
                      sigNum, getSignalName(sigNum));
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    if (prevHandler != nullptr) {
        *prevHandler = prevHandlerLocal;
    }
#else
    struct sigaction sa = {};
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;

    // prepare place holder for previous handler
    struct sigaction prevHandlerLocal = {};
    memset(&prevHandlerLocal, 0, sizeof(struct sigaction));

    // install signal handler
    int res = sigaction(sigNum, &sa, &prevHandlerLocal);
    if (res != 0) {
        LOG_SYS_ERROR(sLogger, sigaction, "Failed to register signal handler for signal %d (%s)",
                      sigNum, getSignalName(sigNum));
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    if (prevHandler != nullptr) {
        *prevHandler = prevHandlerLocal;
    }
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr LinuxExceptionHandler::restoreSignalHandler(int sigNum, SignalHandler& handler) {
#ifdef DBGUTIL_MINGW
    SignalHandler prevHandlerLocal = signal(sigNum, handler);
    if (prevHandlerLocal == SIG_ERR) {
        LOG_SYS_ERROR(sLogger, signal, "Failed to register signal handler for signal %d (%s)",
                      sigNum, getSignalName(sigNum));
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
#else
    // install previous signal handler
    int res = sigaction(sigNum, &handler, nullptr);
    if (res != 0) {
        LOG_SYS_ERROR(sLogger, sigaction, "Failed to register signal handler for signal %d (%s)",
                      sigNum, getSignalName(sigNum));
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr LinuxExceptionHandler::registerSignalHandler(int sigNum) {
    // register handler
#ifdef DBGUTIL_MINGW
    SignalHandler prevHandler = nullptr;
#else
    SignalHandler prevHandler = {};
#endif
    LibDbgErr res =
        registerSignalHandler(sigNum, &LinuxExceptionHandler::signalHandlerStatic, &prevHandler);
    if (res != LIBDBG_ERR_OK) {
        return res;
    }

    // save previous handler
    if (!m_prevHandlerMap.insert(SigHandlerMap::value_type(sigNum, prevHandler)).second) {
        LOG_FATAL(sLogger,
                  "Internal error, duplicate registration of signal handler for signal %d (%s)",
                  sigNum, getSignalName(sigNum));
        // best effort to restore previous handler
        restoreSignalHandler(sigNum, prevHandler);
        return LIBDBG_ERR_ALREADY_EXISTS;
    }
    return LIBDBG_ERR_OK;
}

LibDbgErr LinuxExceptionHandler::unregisterSignalHandler(int sigNum) {
    // find previous handler
    SigHandlerMap::iterator itr = m_prevHandlerMap.find(sigNum);
    if (itr == m_prevHandlerMap.end()) {
        LOG_ERROR(sLogger,
                  "Internal error, could not find previous signal handler for signal %d (%s)",
                  sigNum, getSignalName(sigNum));
        return LIBDBG_ERR_NOT_FOUND;
    }

    // restore previous handler
    SignalHandler prevHandler = itr->second;
    LibDbgErr res = restoreSignalHandler(sigNum, prevHandler);
    if (res != LIBDBG_ERR_OK) {
        LOG_SYS_ERROR(sLogger, signal, "Failed to unregister signal handler for signal %d (%s)",
                      sigNum, getSignalName(sigNum));
        // continue, so we can clear the map entry
    }
    m_prevHandlerMap.erase(itr);
    return res;
}

LibDbgErr LinuxExceptionHandler::registerExceptionHandlers() {
    LibDbgErr rc = registerSignalHandler(SIGSEGV);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    rc = registerSignalHandler(SIGILL);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    rc = registerSignalHandler(SIGFPE);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
#ifdef DBGUTIL_LINUX
    rc = registerSignalHandler(SIGBUS);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    rc = registerSignalHandler(SIGTRAP);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr LinuxExceptionHandler::unregisterExceptionHandlers() {
    LibDbgErr rc = unregisterSignalHandler(SIGSEGV);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    rc = unregisterSignalHandler(SIGILL);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    rc = unregisterSignalHandler(SIGFPE);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
#ifdef DBGUTIL_LINUX
    rc = unregisterSignalHandler(SIGBUS);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    rc = unregisterSignalHandler(SIGTRAP);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
#endif
    return LIBDBG_ERR_OK;
}

void LinuxExceptionHandler::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) LinuxExceptionHandler();
    assert(sInstance != nullptr);
}

LinuxExceptionHandler* LinuxExceptionHandler::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void LinuxExceptionHandler::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

LibDbgErr LinuxExceptionHandler::initializeEx() {
    // code that was compiled under MinGW can run on windows console or on MinGW console, so we
    // distinguish the cases by MSYSTEM environment variable
    // we take the same consideration also in Win32ExceptionHandler
#ifdef DBGUTIL_MINGW
    if (getenv("MSYSTEM") != nullptr) {
        if (getGlobalFlags() & LIBDBG_CATCH_EXCEPTIONS) {
            LOG_DEBUG(sLogger, "Registering signal handler for MinGW under MSYSTEM runtime");
            registerExceptionHandlers();
        }
    } else {
        LOG_DEBUG(sLogger, "Signal handler for MinGW not registered, not under MSYSTEM runtime");
    }
#else
    if (getGlobalFlags() & LIBDBG_CATCH_EXCEPTIONS) {
        registerExceptionHandlers();
    }
#endif

    return LIBDBG_ERR_OK;
}

LibDbgErr LinuxExceptionHandler::terminateEx() {
#ifdef DBGUTIL_MINGW
    if (getenv("MSYSTEM") != nullptr) {
        LOG_DEBUG(sLogger, "Unregistering signal handler for MinGW under MSYSTEM runtime");
        unregisterExceptionHandlers();
    }
#else
    unregisterExceptionHandlers();
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr initLinuxExceptionHandler() {
    // code that was compiled under MinGW can run on windows console or on MinGW console, so we
    // distinguish the cases by MSYSTEM environment variable
    // we take the same consideration also in LinuxExceptionHandler
    registerLogger(sLogger, "linux_exception_handler");
    LinuxExceptionHandler::createInstance();
    LibDbgErr rc = LinuxExceptionHandler::getInstance()->initialize();
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
#ifndef DBGUTIL_MSVC
    setExceptionHandler(LinuxExceptionHandler::getInstance());
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr termLinuxExceptionHandler() {
#ifndef DBGUTIL_MSVC
    setExceptionHandler(nullptr);
#endif
    LibDbgErr rc = LinuxExceptionHandler::getInstance()->terminate();
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    LinuxExceptionHandler::destroyInstance();
    unregisterLogger(sLogger);
    return LIBDBG_ERR_OK;
}

}  // namespace libdbg

#endif  // not defined DBGUTIL_GCC
