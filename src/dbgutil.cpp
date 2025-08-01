#include "libdbg.h"

#ifdef LIBDBG_MSVC
#include "win32_exception_handler.h"
#include "win32_module_manager.h"
#include "win32_stack_trace.h"
#include "win32_symbol_engine.h"
#include "win32_thread_manager.h"
#elif defined(LIBDBG_MINGW)
#include "linux_exception_handler.h"
#include "linux_stack_trace.h"
#include "linux_symbol_engine.h"
#include "linux_thread_manager.h"
#include "win32_exception_handler.h"
#include "win32_module_manager.h"
#include "win32_stack_trace.h"
#include "win32_symbol_engine.h"
#include "win32_thread_manager.h"
#else
#include "elf_reader.h"
#include "linux_exception_handler.h"
#include "linux_module_manager.h"
#include "linux_stack_trace.h"
#include "linux_symbol_engine.h"
#include "linux_thread_manager.h"
#endif

#include "buffered_file_reader.h"
#include "dbgutil_tls.h"
#include "dir_scanner.h"
#include "dwarf_line_util.h"
#include "dwarf_util.h"
#include "elf_reader.h"
#include "libdbg_log_imp.h"
#include "os_image_reader.h"
#include "os_util.h"
#include "path_parser.h"
#include "win32_pe_reader.h"

namespace libdbg {

#ifdef LIBDBG_WINDOWS
static LibDbgErr initWin32DbgUtil();
static LibDbgErr termWin32DbgUtil();
#endif

#ifndef LIBDBG_MSVC
static LibDbgErr initLinuxDbgUtil();
static LibDbgErr termLinuxDbgUtil();
#endif

// helper macro
#define EXEC_CHECK_OP(op)          \
    {                              \
        LibDbgErr rc = op();       \
        if (rc != LIBDBG_ERR_OK) { \
            return rc;             \
        }                          \
    }

LibDbgErr initLibDbg(OsExceptionListener* exceptionListener /* = nullptr */,
                     LogHandler* logHandler /* = nullptr */, LogSeverity severity /* = LS_FATAL */,
                     uint32_t flags /* = 0 */) {
    // TLS and logger initialization is tricky, and must be done in parts
    initLog(logHandler, severity);
    initTls();
    EXEC_CHECK_OP(finishInitLog);
    setGlobalFlags(flags);

#ifdef LIBDBG_WINDOWS
    EXEC_CHECK_OP(initWin32DbgUtil);
#endif

#ifndef LIBDBG_MSVC
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

    return LIBDBG_ERR_OK;
}

LibDbgErr termLibDbg() {
    PathParser::termLogger();
    BufferedFileReader::termLogger();
    DirScanner::termLogger();
    DwarfLineUtil::termLogger();
    DwarfUtil::termLogger();
    OsImageReader::termLogger();
    OsUtil::termLogger();

#ifndef LIBDBG_MSVC
    EXEC_CHECK_OP(termLinuxDbgUtil);
#endif

#ifdef LIBDBG_WINDOWS
    EXEC_CHECK_OP(termWin32DbgUtil);
#endif

    EXEC_CHECK_OP(beginTermLog);
    termTls();
    EXEC_CHECK_OP(termLog);
    return LIBDBG_ERR_OK;
}

#ifdef LIBDBG_WINDOWS
LibDbgErr initWin32DbgUtil() {
    EXEC_CHECK_OP(initWin32ModuleManager);
    EXEC_CHECK_OP(initWin32SymbolEngine);
    EXEC_CHECK_OP(initWin32ExceptionHandler);
    EXEC_CHECK_OP(initWin32ThreadManager);
    EXEC_CHECK_OP(initWin32StackTrace);
    EXEC_CHECK_OP(initWin32PEReader);
    return LIBDBG_ERR_OK;
}
#endif

#ifndef LIBDBG_MSVC
LibDbgErr initLinuxDbgUtil() {
    EXEC_CHECK_OP(initLinuxExceptionHandler);
#ifdef LIBDBG_LINUX
    EXEC_CHECK_OP(initLinuxModuleManager);
#endif
    EXEC_CHECK_OP(initLinuxSymbolEngine);
    EXEC_CHECK_OP(initLinuxThreadManager);
    EXEC_CHECK_OP(initLinuxStackTrace);
#ifdef LIBDBG_LINUX
    EXEC_CHECK_OP(initElfReader);
#endif
    return LIBDBG_ERR_OK;
}
#endif

#ifdef LIBDBG_WINDOWS
LibDbgErr termWin32DbgUtil() {
    EXEC_CHECK_OP(termWin32PEReader);
    EXEC_CHECK_OP(termWin32StackTrace);
    EXEC_CHECK_OP(termWin32ExceptionHandler);
    EXEC_CHECK_OP(termWin32ThreadManager);
    EXEC_CHECK_OP(termWin32SymbolEngine);
    EXEC_CHECK_OP(termWin32ModuleManager);
    return LIBDBG_ERR_OK;
}
#endif

#ifndef LIBDBG_MSVC
LibDbgErr termLinuxDbgUtil() {
#ifdef LIBDBG_LINUX
    EXEC_CHECK_OP(termElfReader);
#endif
    EXEC_CHECK_OP(termLinuxStackTrace);
    EXEC_CHECK_OP(termLinuxThreadManager);
    EXEC_CHECK_OP(termLinuxSymbolEngine);
#ifdef LIBDBG_LINUX
    EXEC_CHECK_OP(termLinuxModuleManager);
#endif
    EXEC_CHECK_OP(termLinuxExceptionHandler);
    return LIBDBG_ERR_OK;
}
#endif

os_thread_id_t getCurrentThreadId() { return OsUtil::getCurrentThreadId(); }

}  // namespace libdbg