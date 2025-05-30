#define DBGUTIL_NO_WINDOWS_HEADER
#include "dbg_util_def.h"

// it turns out that when running from within MinGW/UCRT console, then signal handlers can be
// registered, but when running from Windows console Windows exception handler is in effect.
// this can be distinguished by the existence of the MSYSTEM environment variable
#ifdef DBGUTIL_GCC

#include <signal.h>
#include <string.h>

#include <cinttypes>

#include "dbg_stack_trace.h"
#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "linux_stack_trace.h"

namespace dbgutil {

static Logger sLogger;

static void printFaultAddress(int sigNum, void* addr) {
    switch (sigNum) {
        case SIGSEGV:
        case SIGILL:
        case SIGFPE:
#ifdef DBGUTIL_LINUX
        case SIGBUS:
        case SIGTRAP:
#endif
            // si_addr contains fault address
            fprintf(stderr, "Faulting address: %p\n", addr);
            break;
    }
}

#ifdef DBGUTIL_LINUX
static void printSigIllInfo(int code) {
    switch (code) {
        case ILL_ILLOPC:
            fprintf(stderr, "Illegal opcode\n");
            break;

        case ILL_ILLOPN:
            fprintf(stderr, "Illegal operand\n");
            break;

        case ILL_ILLADR:
            fprintf(stderr, "Illegal addressing mode\n");
            break;

        case ILL_PRVOPC:
            fprintf(stderr, "Privileged opcode\n");
            break;

        case ILL_COPROC:
            fprintf(stderr, "Coprocessor error\n");
            break;

        case ILL_BADSTK:
            fprintf(stderr, "Internal stack error\n");
            break;
    }
}

static void printSigFpeInfo(int code) {
    switch (code) {
        case FPE_INTDIV:
            fprintf(stderr, "Integer division by zero\n");
            break;

        case FPE_INTOVF:
            fprintf(stderr, "Integer overflow\n");
            break;

        case FPE_FLTDIV:
            fprintf(stderr, "Floating-point divide by zero\n");
            break;

        case FPE_FLTOVF:
            fprintf(stderr, "Floating-point overflow\n");
            break;

        case FPE_FLTUND:
            fprintf(stderr, "Floating-point underflow\n");
            break;

        case FPE_FLTRES:
            fprintf(stderr, "Floating-point inexact result\n");
            break;

        case FPE_FLTINV:
            fprintf(stderr, "Floating-point invalid operation\n");
            break;

        case FPE_FLTSUB:
            fprintf(stderr, "Subscript out of range\n");
            break;
    }
}

static void printSigSegvInfo(int code) {
    switch (code) {
        case SEGV_MAPERR:
            fprintf(stderr, "Address not mapped to object\n");
            break;

        case SEGV_ACCERR:
            fprintf(stderr, "Invalid permissions for mapped object\n");
            break;
    }
}

static void printSigBusInfo(int code) {
    switch (code) {
        case BUS_ADRALN:
            fprintf(stderr, "Invalid address alignment\n");
            break;

        case BUS_ADRERR:
            fprintf(stderr, "Nonexistent physical address\n");
            break;

        case BUS_OBJERR:
            fprintf(stderr, "Object - specific hardware error\n");
            break;

        case BUS_MCEERR_AR:
            fprintf(stderr, "Hardware memory error consumed on a machine check\n");
            break;
    }
}

static void printSigInfo(int sigNum, int code) {
    switch (sigNum) {
        case SIGILL:
            printSigIllInfo(code);
            break;

        case SIGFPE:
            printSigFpeInfo(code);
            break;

        case SIGSEGV:
            printSigSegvInfo(code);
            break;

        case SIGBUS:
            printSigBusInfo(code);
            break;
    }
}
#endif

#ifdef DBGUTIL_MINGW
static void signalHandler(int sigNum) {
    fprintf(stderr, "Received signal: %d\n", sigNum);
    dumpStackTrace();
    fprintf(stderr, "Stack trace dumped\n");
}
#else
static void signalHandler(int sigNum, siginfo_t* sigInfo, void* context) {
    fprintf(stderr, "Received signal: %s\n", strsignal(sigNum));

    // printf fault address if relevant
    printFaultAddress(sigNum, sigInfo->si_addr);

    // print additional information
    printSigInfo(sigNum, sigInfo->si_code);

    // dump stack trace
    StderrStackEntryPrinter stderrPrinter;
    // LogStackEntryPrinter logPrinter(LOG_FATAL, "Faulting stack trace");
    // MultiStackEntryPrinter printer(&stderrPrinter, &logPrinter);
    DefaultStackEntryFormatter formatter;
    printStackTraceContext(context, 1, &stderrPrinter, &formatter);
    // dumpStackTraceContext(context, 0);
}
#endif

#ifdef DBGUTIL_MINGW
static DbgUtilErr registerSignalHandler(int sigNum) {
    signal(sigNum, signalHandler);
    return DBGUTIL_ERR_OK;
}
#else
static DbgUtilErr registerSignalHandler(int sigNum) {
    struct sigaction sa = {};
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_SIGINFO;
    int res = sigaction(sigNum, &sa, nullptr);
    if (res != 0) {
        LOG_SYS_ERROR(sLogger, sigaction, "Failed to register signal handler");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}
#endif

#ifdef DBGUTIL_MINGW
static DbgUtilErr unregisterSignalHandler(int sigNum) {
    signal(sigNum, nullptr);
    return DBGUTIL_ERR_OK;
}
#else
static DbgUtilErr unregisterSignalHandler(int sigNum) {
    struct sigaction sa = {};
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = nullptr;
    sa.sa_flags = SA_SIGINFO;
    int res = sigaction(sigNum, &sa, nullptr);
    if (res != 0) {
        LOG_SYS_ERROR(sLogger, sigaction, "Failed to unregister signal handler");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}
#endif

static DbgUtilErr linuxRegisterExceptionHandler() {
    DbgUtilErr rc = registerSignalHandler(SIGSEGV);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    rc = registerSignalHandler(SIGILL);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    rc = registerSignalHandler(SIGFPE);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
#ifdef DBGUTIL_LINUX
    rc = registerSignalHandler(SIGBUS);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    rc = registerSignalHandler(SIGTRAP);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
#endif
    return DBGUTIL_ERR_OK;
}

static void linuxUnregisterExceptionHandler() {
    unregisterSignalHandler(SIGSEGV);
    unregisterSignalHandler(SIGILL);
    unregisterSignalHandler(SIGFPE);
#ifdef DBGUTIL_LINUX
    unregisterSignalHandler(SIGBUS);
    unregisterSignalHandler(SIGTRAP);
#endif
}

DbgUtilErr initLinuxExceptionHandler() {
    // code that was compiled under MinGW can run on windows console or on MinGW console, so we
    // distinguish the cases by MSYSTEM environment variable
    // we take the same consideration also in Win32ExceptionHandler
    registerLogger(sLogger, "linux_exception_handler");
#ifdef DBGUTIL_MINGW
    if (getenv("MSYSTEM") != nullptr) {
        LOG_DEBUG(sLogger, "Registering signal handler for MinGW under MSYSTEM runtime");
        linuxRegisterExceptionHandler();
        std::set_terminate([]() {
            dumpStackTrace();
            abort();
        });
    } else {
        LOG_DEBUG(sLogger, "Signal handler for MinGW not registered, not under MSYSTEM runtime");
    }
#else
    linuxRegisterExceptionHandler();
#endif
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLinuxExceptionHandler() {
#ifdef DBGUTIL_MINGW
    if (getenv("MSYSTEM") != nullptr) {
        LOG_DEBUG(sLogger, "Unregistering signal handler for MinGW under MSYSTEM runtime");
        linuxUnregisterExceptionHandler();
    }
#else
    linuxUnregisterExceptionHandler();
#endif
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

#if 0
BEGIN_STARTUP_JOB(LinuxExceptionHandler) {
    // code that was compiled under MinGW can run on windows console or on MinGW console, so we
    // distinguish the cases by MSYSTEM environment variable
    // we take the same consideration also in Win32ExceptionHandler
    sLogger = elog::ELogSystem::getSharedLogger("csi.common.linux_exception_handler");
#ifdef DBGUTIL_MINGW
    if (getenv("MSYSTEM") != nullptr) {
        LOG_DEBUG(sLogger, "Registering signal handler for MinGW under MSYSTEM runtime");
        linuxRegisterExceptionHandler();
        std::set_terminate([]() {
            dumpStackTrace();
            abort();
        });
    } else {
        LOG_DEBUG(sLogger,
                      "Signal handler for MinGW not registered, not under MSYSTEM runtime");
    }
#else
    linuxRegisterExceptionHandler();
#endif
    return DBGUTIL_ERR_OK;
}
END_STARTUP_JOB(LinuxExceptionHandler)

BEGIN_TEARDOWN_JOB(LinuxExceptionHandler) {
#ifdef DBGUTIL_MINGW
    if (getenv("MSYSTEM") != nullptr) {
        LOG_DEBUG(sLogger, "Unregistering signal handler for MinGW under MSYSTEM runtime");
        linuxUnregisterExceptionHandler();
    }
#else
    linuxUnregisterExceptionHandler();
#endif
    return DBGUTIL_ERR_OK;
}
END_TEARDOWN_JOB(LinuxExceptionHandler)
#endif

}  // namespace dbgutil

#endif  // not defined DBGUTIL_GCC
