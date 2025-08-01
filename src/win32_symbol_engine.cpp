// avoid including windows header, otherwise MinGW compilation fails
#define LIBDBG_NO_WINDOWS_HEADER
#include "libdbg_def.h"

// this is good only for Microsoft Visual C++ compiler
// Although this code compiles on MinGW, the g++ compiler does not generate a pdb symbol file
// so most symbol engine functions fail, but this is still useful for stack walking, without symbol
// extraction, which takes place in bfd symbol engine.
#ifdef LIBDBG_WINDOWS

// we put this in ifdef so that vscode auto-format will not move it around...
#ifdef LIBDBG_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <dbghelp.h>
#include <psapi.h>

#include <cassert>
#include <cinttypes>

#include "dbg_stack_trace.h"
#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "path_parser.h"
#include "win32_module_manager.h"
#include "win32_symbol_engine.h"

namespace libdbg {

static Logger sLogger;

Win32SymbolEngine* Win32SymbolEngine::sInstance = nullptr;

void Win32SymbolEngine::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) Win32SymbolEngine();
    assert(sInstance != nullptr);
}

Win32SymbolEngine* Win32SymbolEngine::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void Win32SymbolEngine::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

LibDbgErr Win32SymbolEngine::initialize() {
    // get current process handle
    Win32ModuleManager* moduleManager = Win32ModuleManager::getInstance();
    m_processHandle = moduleManager->getProcessHandle();

    // get current process image path from main module
    OsModuleInfo mainModuleInfo;
    LibDbgErr rc = moduleManager->getMainModule(mainModuleInfo);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }

    // get the containing directory
    LOG_TRACE(sLogger, "Main module file path is: %s", mainModuleInfo.m_modulePath.c_str());
    rc = PathParser::getParentPath(mainModuleInfo.m_modulePath.c_str(), m_processDir);
    if (rc != LIBDBG_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to extract parent path from module '%s': %s",
                  mainModuleInfo.m_modulePath.c_str(), errorCodeToStr(rc));
        return rc;
    }
    LOG_TRACE(sLogger, "Process directory is: %s", m_processDir.c_str());

    // get process name
    rc = PathParser::getFileName(mainModuleInfo.m_modulePath.c_str(), m_processName);
    if (rc != LIBDBG_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to extract file name from module '%s': %s",
                  mainModuleInfo.m_modulePath.c_str(), errorCodeToStr(rc));
        return rc;
    }
    std::string::size_type lastDotPos = m_processName.find_last_of('.');
    if (lastDotPos != std::string::npos) {
        m_processName = m_processName.substr(0, lastDotPos);
    }
    LOG_TRACE(sLogger, "Process name is: %s", m_processName.c_str());

    // initialize the symbol engine
    if (!SymInitialize(m_processHandle, m_processDir.c_str(), TRUE)) {
        LOG_SYS_ERROR(sLogger, SymInitialize,
                      "Cannot initialize symbol handler: failed to initialize debug symbol engine");
        m_processHandle = INVALID_HANDLE_VALUE;
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    LOG_TRACE(sLogger, "Symbol engine initialized");

    // set symbol engine options
    DWORD symOptions = SymGetOptions();
    symOptions |= SYMOPT_LOAD_LINES;
    symOptions |= SYMOPT_UNDNAME;
    SymSetOptions(symOptions);

    // get the image type (required for stack walk only)
    IMAGE_NT_HEADERS* headers = ImageNtHeader(mainModuleInfo.m_loadAddress);
    if (headers == nullptr) {
        LOG_SYS_ERROR(sLogger, ImageNtHeader, "Failed to get process image headers");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    m_imageType = headers->FileHeader.Machine;
    return LIBDBG_ERR_OK;
}

LibDbgErr Win32SymbolEngine::terminate() {
    if (!SymCleanup(m_processHandle)) {
        LOG_SYS_ERROR(sLogger, SymCleanup, "Failed to terminate debug symbol engine");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    m_processHandle = INVALID_HANDLE_VALUE;
    return LIBDBG_ERR_OK;
}

#ifdef _M_X64
static void initStackFrame(const CONTEXT& context, STACKFRAME64& stackFrame) {
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
}
#else
static void initStackFrame(const CONTEXT& context, STACKFRAME64& stackFrame) {
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
}
#endif

// this implementation is available also for MinGW, as it might interact with non-gcc modules
LibDbgErr Win32SymbolEngine::getSymbolInfo(void* symAddress, SymbolInfo& symbolInfo) {
    // prepare symbol info struct
    const int MAX_NAME_LEN = 1024;
    IMAGEHLP_SYMBOL64* sym = (IMAGEHLP_SYMBOL64*)malloc(sizeof(IMAGEHLP_SYMBOL64) + MAX_NAME_LEN);
    memset(sym, '\0', sizeof(IMAGEHLP_SYMBOL64) + MAX_NAME_LEN);
    sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    sym->MaxNameLength = MAX_NAME_LEN;
    DWORD64 displacement = 0;

    // get symbol info
    if (!SymGetSymFromAddr64(m_processHandle, (DWORD64)symAddress, &displacement, sym)) {
        DWORD rc = GetLastError();
        LOG_TRACE(sLogger, "Failed to get debug symbol for address 0x%" PRIx64 " (error code: %u)",
                  symAddress, rc);
        free(sym);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }

    // un-decorate symbol name
    std::vector<char> nameBuf(MAX_NAME_LEN);
    DWORD nameLen = UnDecorateSymbolName(sym->Name, &nameBuf[0], MAX_NAME_LEN, UNDNAME_COMPLETE);
    if (nameLen == 0) {
        LOG_SYS_ERROR(sLogger, SymGetSymFromAddr64, "Failed to get undecorated name for %s",
                      sym->Name);
        symbolInfo.m_symbolName = sym->Name;
    } else {
        symbolInfo.m_symbolName = std::string(&nameBuf[0], nameLen);
    }

    // get file/line info
    DWORD offsetFromSymbol = 0;
    IMAGEHLP_LINE64 lineInfo = {0};
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    if (SymGetLineFromAddr64(m_processHandle, (DWORD64)symAddress, &offsetFromSymbol, &lineInfo)) {
        symbolInfo.m_fileName = lineInfo.FileName;
        symbolInfo.m_lineNumber = lineInfo.LineNumber;
        symbolInfo.m_startAddress = (void*)lineInfo.Address;
        symbolInfo.m_byteOffset =
            (uint32_t)displacement;  //(uint32_t)(lineInfo.Address - (DWORD64)symAddress);
    }
    free(sym);

    return getSymbolModule(symAddress, symbolInfo);
}

void Win32SymbolEngine::walkThreadStack(HANDLE hThread, CONTEXT& context,
                                        StackFrameListener* listener) {
    STACKFRAME64 stackFrame = {};
    initStackFrame(context, stackFrame);

    do {
        if (!StackWalk64(m_imageType, m_processHandle, hThread, &stackFrame, &context, NULL,
                         SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            break;
        }

        void* addr = nullptr;
        if (stackFrame.AddrPC.Offset != 0) {
            addr = (void*)stackFrame.AddrPC.Offset;
            listener->onStackFrame(addr);
        }
    } while (stackFrame.AddrReturn.Offset != 0);
}

LibDbgErr Win32SymbolEngine::walkStack(StackFrameListener* listener, void* context) {
    HANDLE thread = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &thread, 0,
                         false, DUPLICATE_SAME_ACCESS)) {
        LOG_SYS_ERROR(sLogger, DuplicateHandle,
                      "Cannot print stack trace: failed to duplicate current thread handle");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }

    LOG_DEBUG(sLogger, "Dumping stack trace");
    walkThreadStack(thread, *(PCONTEXT)context, listener);
    LOG_DEBUG(sLogger, "Done dumping stack trace");
    CloseHandle(thread);
    return LIBDBG_ERR_OK;
}

Win32SymbolEngine::Win32SymbolEngine() : m_processHandle(INVALID_HANDLE_VALUE), m_imageType(0) {}

LibDbgErr Win32SymbolEngine::getSymbolModule(void* symAddress, SymbolInfo& symbolInfo) {
    OsModuleInfo moduleInfo;
    LibDbgErr rc = getModuleManager()->getModuleByAddress(symAddress, moduleInfo);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    symbolInfo.m_moduleName = moduleInfo.m_modulePath;
    symbolInfo.m_moduleBaseAddress = moduleInfo.m_loadAddress;
    return LIBDBG_ERR_OK;
}

void Win32SymbolEngine::dumpCore(void* ExceptionInfo) {
    // try to create mini-dump
    std::stringstream s;
    s << m_processDir << "\\" << m_processName << ".core." << _getpid();
    std::string dumpPath = s.str();
    LOG_TRACE(sLogger, "Attempting to generate mini-dump at %s", dumpPath.c_str());
    HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, CreateFileA, "Failed to create dump file: %s", dumpPath.c_str());
        return;
    }
    _MINIDUMP_EXCEPTION_INFORMATION mdExceptInfo = {GetThreadId(GetCurrentThread()),
                                                    (_EXCEPTION_POINTERS*)ExceptionInfo, FALSE};
    if (!MiniDumpWriteDump(m_processHandle, GetCurrentProcessId(), hFile, MiniDumpWithFullMemory,
                           &mdExceptInfo, NULL, NULL)) {
        LOG_WIN32_ERROR(sLogger, MiniDumpWriteDump, "Failed to write mini-dump file");
    }
    CloseHandle(hFile);
}

LibDbgErr initWin32SymbolEngine() {
    registerLogger(sLogger, "win32_symbol_engine");
    Win32SymbolEngine::createInstance();
    LibDbgErr rc = Win32SymbolEngine::getInstance()->initialize();
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
#ifdef LIBDBG_MSVC
    setSymbolEngine(Win32SymbolEngine::getInstance());
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr termWin32SymbolEngine() {
#ifdef LIBDBG_MSVC
    setSymbolEngine(nullptr);
#endif
    LibDbgErr rc = Win32SymbolEngine::getInstance()->terminate();
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    Win32SymbolEngine::destroyInstance();
    unregisterLogger(sLogger);
    return LIBDBG_ERR_OK;
}

}  // namespace libdbg

#endif  // LIBDBG_WINDOWS