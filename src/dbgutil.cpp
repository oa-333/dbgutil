#include "dbg_util.h"

#ifdef DBGUTIL_MSVC
#include "win32_exception_handler.h"
#include "win32_life_sign_manager.h"
#include "win32_module_manager.h"
#include "win32_stack_trace.h"
#include "win32_symbol_engine.h"
#include "win32_thread_manager.h"
#elif defined(DBGUTIL_MINGW)
#include "linux_exception_handler.h"
#include "linux_stack_trace.h"
#include "linux_symbol_engine.h"
#include "linux_thread_manager.h"
#include "win32_exception_handler.h"
#include "win32_life_sign_manager.h"
#include "win32_module_manager.h"
#include "win32_stack_trace.h"
#include "win32_symbol_engine.h"
#include "win32_thread_manager.h"
#else
#include "elf_reader.h"
#include "linux_exception_handler.h"
#include "linux_life_sign_manager.h"
#include "linux_module_manager.h"
#include "linux_stack_trace.h"
#include "linux_symbol_engine.h"
#include "linux_thread_manager.h"
#endif

#include "buffered_file_reader.h"
#include "dbgutil_log_imp.h"
#include "dbgutil_tls.h"
#include "dir_scanner.h"
#include "dwarf_line_util.h"
#include "dwarf_util.h"
#include "elf_reader.h"
#include "os_image_reader.h"
#include "os_util.h"
#include "path_parser.h"
#include "win32_pe_reader.h"

namespace dbgutil {

static bool sIsInitialized = false;

#ifdef DBGUTIL_WINDOWS
static DbgUtilErr initWin32DbgUtil();
static DbgUtilErr termWin32DbgUtil();
#endif

#ifndef DBGUTIL_MSVC
static DbgUtilErr initLinuxDbgUtil();
static DbgUtilErr termLinuxDbgUtil();
#endif

// helper macro
#define EXEC_CHECK_OP(op)           \
    {                               \
        DbgUtilErr rc = op();       \
        if (rc != DBGUTIL_ERR_OK) { \
            return rc;              \
        }                           \
    }

DbgUtilErr initDbgUtil(OsExceptionListener* exceptionListener /* = nullptr */,
                       LogHandler* logHandler /* = nullptr */,
                       LogSeverity severity /* = LS_FATAL */, uint32_t flags /* = 0 */) {
    if (sIsInitialized) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    // TLS and logger initialization is tricky, and must be done in parts
    initLog(logHandler, severity);
    initTls();
    EXEC_CHECK_OP(finishInitLog);
    setGlobalFlags(flags);

#ifdef DBGUTIL_WINDOWS
    EXEC_CHECK_OP(initWin32DbgUtil);
#endif

#ifndef DBGUTIL_MSVC
    EXEC_CHECK_OP(initLinuxDbgUtil);
#endif
    if (exceptionListener != nullptr) {
        getExceptionHandler()->setExceptionListener(exceptionListener);
    }

    PathParser::initLogger();
    BufferedFileReader::initLogger();
    DirScanner::initLogger();
    DwarfLineUtil::initLogger();
    DwarfUtil::initLogger();
    OsImageReader::initLogger();
    OsUtil::initLogger();

    sIsInitialized = true;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termDbgUtil() {
    if (!sIsInitialized) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    PathParser::termLogger();
    BufferedFileReader::termLogger();
    DirScanner::termLogger();
    DwarfLineUtil::termLogger();
    DwarfUtil::termLogger();
    OsImageReader::termLogger();
    OsUtil::termLogger();

#ifndef DBGUTIL_MSVC
    EXEC_CHECK_OP(termLinuxDbgUtil);
#endif

#ifdef DBGUTIL_WINDOWS
    EXEC_CHECK_OP(termWin32DbgUtil);
#endif

    EXEC_CHECK_OP(beginTermLog);
    termTls();
    EXEC_CHECK_OP(termLog);
    sIsInitialized = false;
    return DBGUTIL_ERR_OK;
}

bool isDbgUtilInitialized() { return sIsInitialized; }

#ifdef DBGUTIL_WINDOWS
DbgUtilErr initWin32DbgUtil() {
    EXEC_CHECK_OP(initWin32ModuleManager);
    EXEC_CHECK_OP(initWin32SymbolEngine);
    EXEC_CHECK_OP(initWin32ExceptionHandler);
    EXEC_CHECK_OP(initWin32ThreadManager);
    EXEC_CHECK_OP(initWin32StackTrace);
    EXEC_CHECK_OP(initWin32PEReader);
    EXEC_CHECK_OP(initWin32LifeSignManager);
    return DBGUTIL_ERR_OK;
}
#endif

#ifndef DBGUTIL_MSVC
DbgUtilErr initLinuxDbgUtil() {
    EXEC_CHECK_OP(initLinuxExceptionHandler);
#ifdef DBGUTIL_LINUX
    EXEC_CHECK_OP(initLinuxModuleManager);
#endif
    EXEC_CHECK_OP(initLinuxSymbolEngine);
    EXEC_CHECK_OP(initLinuxThreadManager);
    EXEC_CHECK_OP(initLinuxStackTrace);
#ifdef DBGUTIL_LINUX
    EXEC_CHECK_OP(initElfReader);
    EXEC_CHECK_OP(initLinuxLifeSignManager);
#endif
    return DBGUTIL_ERR_OK;
}
#endif

#ifdef DBGUTIL_WINDOWS
DbgUtilErr termWin32DbgUtil() {
    EXEC_CHECK_OP(termWin32LifeSignManager);
    EXEC_CHECK_OP(termWin32PEReader);
    EXEC_CHECK_OP(termWin32StackTrace);
    EXEC_CHECK_OP(termWin32ExceptionHandler);
    EXEC_CHECK_OP(termWin32ThreadManager);
    EXEC_CHECK_OP(termWin32SymbolEngine);
    EXEC_CHECK_OP(termWin32ModuleManager);
    return DBGUTIL_ERR_OK;
}
#endif

#ifndef DBGUTIL_MSVC
DbgUtilErr termLinuxDbgUtil() {
#ifdef DBGUTIL_LINUX
    EXEC_CHECK_OP(termLinuxLifeSignManager);
    EXEC_CHECK_OP(termElfReader);
#endif
    EXEC_CHECK_OP(termLinuxStackTrace);
    EXEC_CHECK_OP(termLinuxThreadManager);
    EXEC_CHECK_OP(termLinuxSymbolEngine);
#ifdef DBGUTIL_LINUX
    EXEC_CHECK_OP(termLinuxModuleManager);
#endif
    EXEC_CHECK_OP(termLinuxExceptionHandler);
    return DBGUTIL_ERR_OK;
}
#endif

os_thread_id_t getCurrentThreadId() { return OsUtil::getCurrentThreadId(); }

}  // namespace dbgutil