#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

// MinGW requires including windows main header, and this should be the topmost include directive
#ifdef DBGUTIL_MINGW
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <cassert>
#include <cinttypes>

#include "dbg_util_flags.h"
#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "os_exception_handler_internal.h"
#include "win32_exception_handler.h"
#include "win32_symbol_engine.h"

namespace dbgutil {

static Logger sLogger;

LPTOP_LEVEL_EXCEPTION_FILTER Win32ExceptionHandler::sPrevFilter = nullptr;

// thread local static buffer for exception information (avoid allocation during panic)
#define EXCEPTION_BUF_SIZE 256
static thread_local char sExceptBuf[EXCEPTION_BUF_SIZE];
static size_t sExceptBufLen = 0;

Win32ExceptionHandler* Win32ExceptionHandler::sInstance = nullptr;

static const char* win32GetExceptionName(DWORD dwExcept) {
    switch (dwExcept) {
        case EXCEPTION_ACCESS_VIOLATION:
            return "EXCEPTION_ACCESS_VIOLATION";

        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";

        case EXCEPTION_BREAKPOINT:
            return "EXCEPTION_BREAKPOINT";

        case EXCEPTION_DATATYPE_MISALIGNMENT:
            return "EXCEPTION_DATATYPE_MISALIGNMENT";

        case EXCEPTION_FLT_DENORMAL_OPERAND:
            return "EXCEPTION_FLT_DENORMAL_OPERAND";

        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            return "EXCEPTION_FLT_DIVIDE_BY_ZERO";

        case EXCEPTION_FLT_INEXACT_RESULT:
            return "EXCEPTION_FLT_INEXACT_RESULT";

        case EXCEPTION_FLT_INVALID_OPERATION:
            return "EXCEPTION_FLT_INVALID_OPERATION";

        case EXCEPTION_FLT_OVERFLOW:
            return "EXCEPTION_FLT_OVERFLOW";

        case EXCEPTION_FLT_STACK_CHECK:
            return "EXCEPTION_FLT_STACK_CHECK";

        case EXCEPTION_FLT_UNDERFLOW:
            return "EXCEPTION_FLT_UNDERFLOW";

        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "EXCEPTION_ILLEGAL_INSTRUCTION";

        case EXCEPTION_IN_PAGE_ERROR:
            return "EXCEPTION_IN_PAGE_ERROR";

        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return "EXCEPTION_INT_DIVIDE_BY_ZERO";

        case EXCEPTION_INT_OVERFLOW:
            return "EXCEPTION_INT_OVERFLOW";

        case EXCEPTION_INVALID_DISPOSITION:
            return "EXCEPTION_INVALID_DISPOSITION";

        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            return "EXCEPTION_NONCONTINUABLE_EXCEPTION";

        case EXCEPTION_PRIV_INSTRUCTION:
            return "EXCEPTION_PRIV_INSTRUCTION";

        case EXCEPTION_SINGLE_STEP:
            return "EXCEPTION_SINGLE_STEP";

        case EXCEPTION_STACK_OVERFLOW:
            return "EXCEPTION_STACK_OVERFLOW";

        case EXCEPTION_GUARD_PAGE:
            return "EXCEPTION_GUARD_PAGE";

        case EXCEPTION_INVALID_HANDLE:
            return "EXCEPTION_INVALID_HANDLE";

        default:
            return "N/A";
    }
}

static const char* getAccessViolationType(ULONG_PTR code) {
    switch (code) {
        case 0:
            return "read";

        case 1:
            return "write";

        default:
            return "N/A";
    }
}

static void getExtendedExceptionInfo(_EXCEPTION_POINTERS* exceptionInfo) {
    // NOTE: preparing extended exception information this (as opposed to using FormatMessage()
    // commented out below), may not be robust, especially with regards to Win32 API changes,
    // but it is undocumented how to pass the last va_list parameter to FormatMessage(). It seems
    // correct to use ExceptionInformation array of EXCEPTION_RECORD (along with its size
    // NumberParameters), but it is unclear whether type conversion should take place from ULONG_PTR
    // (array member type), and this depends on the printf format specifier of the format message,
    // which is clearly unknown. Therefore, a compromise was chosen, to report only access violation
    // and page error (the first being the most frequent exception), and that is because MSDN has
    // documentation for these two (see
    // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record -
    // especially the ExceptionInformation member array documentation).
    if (exceptionInfo->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) {
        int res = snprintf(sExceptBuf + sExceptBufLen, EXCEPTION_BUF_SIZE - sExceptBufLen,
                           "Exception is non-continuable\n");
        if (res > 0) {
            sExceptBufLen += (size_t)res;
        }
    }

    // access violation or page error
    if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
        if (exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
            if (exceptionInfo->ExceptionRecord->ExceptionInformation[0] == 8) {
                int res = snprintf(sExceptBuf + sExceptBufLen, EXCEPTION_BUF_SIZE - sExceptBufLen,
                                   "The instruction at 0x%p referenced memory at 0x%p, causing "
                                   "user-mode data execution prevention (DEP) violation",
                                   exceptionInfo->ExceptionRecord->ExceptionAddress,
                                   (void*)exceptionInfo->ExceptionRecord->ExceptionInformation[1]);
                if (res > 0) {
                    sExceptBufLen += (size_t)res;
                }
            } else {
                int res = snprintf(sExceptBuf + sExceptBufLen, EXCEPTION_BUF_SIZE - sExceptBufLen,
                                   "The instruction at 0x%p referenced memory at 0x%p. The memory "
                                   "could not be %s.",
                                   exceptionInfo->ExceptionRecord->ExceptionAddress,
                                   (void*)exceptionInfo->ExceptionRecord->ExceptionInformation[1],
                                   getAccessViolationType(
                                       exceptionInfo->ExceptionRecord->ExceptionInformation[0]));
                if (res > 0) {
                    sExceptBufLen += (size_t)res;
                }
            }
        }
        if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR &&
            exceptionInfo->ExceptionRecord->NumberParameters >= 3) {
            int res = snprintf(sExceptBuf + sExceptBufLen, EXCEPTION_BUF_SIZE - sExceptBufLen,
                               "NT STATUS code: %ld",  // NTSTATUS is typedef of LONG
                               (LONG)exceptionInfo->ExceptionRecord->ExceptionInformation[3]);
            if (res > 0) {
                sExceptBufLen += (size_t)res;
            }
        }
    }
}

LONG WINAPI
Win32ExceptionHandler::unhandledExceptionFilterStatic(_EXCEPTION_POINTERS* exceptionInfo) noexcept {
    Win32ExceptionHandler* exceptionHandler = (Win32ExceptionHandler*)getExceptionHandler();
    exceptionHandler->unhandledExceptionFilter(exceptionInfo);

    // pass exception to default handling by OS (or another filter installed by user)
    return EXCEPTION_CONTINUE_SEARCH;
}

void Win32ExceptionHandler::unhandledExceptionFilter(_EXCEPTION_POINTERS* exceptionInfo) {
    // prepare exception information
    OsExceptionInfo exInfo;
    exInfo.m_exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;
    exInfo.m_exceptionSubCode = 0;
    exInfo.m_exceptionName = win32GetExceptionName(exInfo.m_exceptionCode);
    exInfo.m_faultAddress = exceptionInfo->ExceptionRecord->ExceptionAddress;

    // orint basic exception information
    sExceptBuf[0] = 0;
    int res =
        snprintf(sExceptBuf, EXCEPTION_BUF_SIZE, "Encountered unhandled exception 0x%08lX: %s\n",
                 exInfo.m_exceptionCode, exInfo.m_exceptionName);
    if (res > 0) {
        sExceptBufLen += (size_t)res;
    }

    // print fault address
    res = snprintf(sExceptBuf + sExceptBufLen, EXCEPTION_BUF_SIZE - sExceptBufLen,
                   "Faulting address: 0x%p\n", exInfo.m_faultAddress);
    if (res > 0) {
        sExceptBufLen += (size_t)res;
    }

    // print extended information if any
    getExtendedExceptionInfo(exceptionInfo);
    exInfo.m_fullExceptionInfo = sExceptBuf;

    // get stack trace information
    exInfo.m_callStack = prepareCallStack(exceptionInfo->ContextRecord);

    // now we can dispatch the exception
    dispatchExceptionInfo(exInfo);

    // nevertheless, we also send to log
    if (getGlobalFlags() & DBGUTIL_LOG_EXCEPTIONS) {
        LOG_FATAL(sLogger, exInfo.m_fullExceptionInfo);
        LOG_FATAL(sLogger, exInfo.m_callStack);
    }

    // finally, attempt to dump core
    if (getGlobalFlags() & DBGUTIL_EXCEPTION_DUMP_CORE) {
        LOG_WARN(sLogger, "Dumping core");
        Win32SymbolEngine::getInstance()->dumpCore(exceptionInfo);
        LOG_WARN(sLogger, "Finished dumping core");
    }

    // old exception printing code, keeping for reference
#if 0
    // print exception code
    DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;
    LOG_ERROR(sLogger, "Encountered unhandled exception: 0x%08X", exceptionCode);

    // log exception message from system
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
        LoadLibraryA("NTDLL.DLL"), exceptionCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, NULL);
    LOG_FATAL(sLogger, "Encountered unhandled exception %" PRIx32 ": %s", exceptionCode,
              messageBuffer);
    LocalFree(messageBuffer);
#endif
}

void Win32ExceptionHandler::registerExceptionHandler() {
    sPrevFilter =
        SetUnhandledExceptionFilter(&Win32ExceptionHandler::unhandledExceptionFilterStatic);
}

void Win32ExceptionHandler::unregisterExceptionHandler() {
    SetUnhandledExceptionFilter(sPrevFilter);
}

void Win32ExceptionHandler::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) Win32ExceptionHandler();
    assert(sInstance != nullptr);
}

Win32ExceptionHandler* Win32ExceptionHandler::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void Win32ExceptionHandler::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

DbgUtilErr Win32ExceptionHandler::initializeEx() {
    // code that was compiled under MinGW can run on windows console or on MinGW console, so we
    // distinguish the cases by MSYSTEM environment variable
    // we take the same consideration also in Win32ExceptionHandler
    const size_t BUF_SIZE = 64;
    char buf[BUF_SIZE];
    size_t retSize = 0;
    (void)getenv_s(&retSize, buf, BUF_SIZE, "MSYSTEM");
    if (retSize != 0) {
        LOG_DEBUG(sLogger,
                  "Exception handler for MinGW not registered, running under MSYSTEM runtime");
    } else {
        if (getGlobalFlags() & DBGUTIL_CATCH_EXCEPTIONS) {
            registerExceptionHandler();
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32ExceptionHandler::terminateEx() {
    const size_t BUF_SIZE = 64;
    char buf[BUF_SIZE];
    size_t retSize = 0;
    (void)getenv_s(&retSize, buf, BUF_SIZE, "MSYSTEM");
    if (retSize != 0) {
        if (getGlobalFlags() & DBGUTIL_CATCH_EXCEPTIONS) {
            unregisterExceptionHandler();
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr initWin32ExceptionHandler() {
    // code that was compiled under MinGW can run on windows console or on MinGW console, so we
    // distinguish the cases by MSYSTEM environment variable
    // we take the same consideration also in Win32ExceptionHandler
    registerLogger(sLogger, "win32_exception_handler");
    Win32ExceptionHandler::createInstance();
    DbgUtilErr rc = Win32ExceptionHandler::getInstance()->initialize();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
#ifdef DBGUTIL_MSVC
    setExceptionHandler(Win32ExceptionHandler::getInstance());
#endif
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termWin32ExceptionHandler() {
#ifdef DBGUTIL_MSVC
    setExceptionHandler(nullptr);
#endif
    DbgUtilErr rc = Win32ExceptionHandler::getInstance()->terminate();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    Win32ExceptionHandler::destroyInstance();
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS
