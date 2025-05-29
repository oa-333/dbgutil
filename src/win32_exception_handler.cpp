#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

// we put this in ifdef so that vscode auto-format will not move it around...
#ifdef DBGUTIL_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cinttypes>

#include "dbg_stack_trace.h"
#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "win32_symbol_engine.h"

namespace dbgutil {

static Logger sLogger;

LONG WINAPI unhandledExceptionFilter(_EXCEPTION_POINTERS* ExceptionInfo) {
    // print exception code
    DWORD exceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
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

    // dump core
    LOG_DEBUG(sLogger, "Dumping core");
    Win32SymbolEngine::getInstance()->dumpCore(ExceptionInfo);
    LOG_DEBUG(sLogger, "Finished dumping core");

    // print call stack
    dumpStackTraceContext(ExceptionInfo->ContextRecord, 1);

    // pass exception to default handling by OS
    return EXCEPTION_CONTINUE_SEARCH;
}

void win32RegisterExceptionHandler() { SetUnhandledExceptionFilter(unhandledExceptionFilter); }
void win32UnregisterExceptionHandler() { SetUnhandledExceptionFilter(NULL); }

DbgUtilErr initWin32ExceptionHandler() {
    // code that was compiled under MinGW can run on windows console or on MinGW console, so we
    // distinguish the cases by MSYSTEM environment variable
    // we take the same consideration also in Win32ExceptionHandler
    registerLogger(sLogger, "win32_exception_handler");
    if (getenv("MSYSTEM") != nullptr) {
        LOG_DEBUG(sLogger,
                  "Exception handler for MinGW not registered, running under MSYSTEM runtime");
    } else {
        win32RegisterExceptionHandler();
        std::set_terminate([]() {
            dumpStackTrace();
            abort();
        });
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termWin32ExceptionHandler() {
    if (getenv("MSYSTEM") == nullptr) {
        win32UnregisterExceptionHandler();
    }
    return DBGUTIL_ERR_OK;
}

#if 0
BEGIN_STARTUP_JOB(Win32ExceptionHandler) {
    // code that was compiled under MinGW can run on windows console or on MinGW console, so we
    // distinguish the cases by MSYSTEM environment variable
    // we take the same consideration also in Win32ExceptionHandler
    sLogger = elog::ELogSystem::getSharedLogger("csi.common.win32_exception_handler");
    if (getenv("MSYSTEM") != nullptr) {
        LOG_DEBUG(sLogger,
                      "Exception handler for MinGW not registered, running under MSYSTEM runtime");
    } else {
        win32RegisterExceptionHandler();
        std::set_terminate([]() {
            dumpStackTrace();
            abort();
        });
    }
    return DBGUTIL_ERR_OK;
}
END_STARTUP_JOB(Win32ExceptionHandler)
DECLARE_STARTUP_JOB_DEP(Win32ExceptionHandler, Win32SymbolEngine)

BEGIN_TEARDOWN_JOB(Win32ExceptionHandler) {
    if (getenv("MSYSTEM") == nullptr) {
        win32UnregisterExceptionHandler();
    }
    return DBGUTIL_ERR_OK;
}
END_TEARDOWN_JOB(Win32ExceptionHandler)
#endif

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS
