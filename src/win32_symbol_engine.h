#ifndef __WIN32_SYM_HANDLER_H__
#define __WIN32_SYM_HANDLER_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

#include "os_stack_trace.h"
#include "os_symbol_engine.h"

namespace libdbg {

class Win32SymbolEngine : public OsSymbolEngine {
public:
    /** @brief Creates the singleton instance of the symbol engine for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the symbol engine. */
    static Win32SymbolEngine* getInstance();

    /** @brief Destroys the singleton instance of the symbol engine. */
    static void destroyInstance();

    /** @brief Initializes the symbol engine. */
    LibDbgErr initialize();

    /** @brief Destroys the symbol engine. */
    LibDbgErr terminate();

    /**
     * @brief Retrieves symbol debug information (platform independent API).
     * @param symAddress The symbol address.
     * @param[out] symbolInfo The symbol information.
     */
    LibDbgErr getSymbolInfo(void* symAddress, SymbolInfo& symbolInfo) final;

    /**
     * @brief Dumps core file.
     * @param ExceptionInfo Pointer to the exception information object (opaque).
     */
    void dumpCore(void* ExceptionInfo);

    /**
     * @brief Implement stack walking.
     * @note Since stack walking is tightly coupled with Windows debug symbol API, the functionality
     * is implemented here and the @ref Win32SymbolEngine delegates to here.
     * @param listener The frame listener.
     * @param context Call context (opaque).
     * @return The operation result.
     */
    LibDbgErr walkStack(StackFrameListener* listener, void* context);

private:
    Win32SymbolEngine();
    Win32SymbolEngine(const Win32SymbolEngine&) = delete;
    Win32SymbolEngine(Win32SymbolEngine&&) = delete;
    Win32SymbolEngine& operator=(const Win32SymbolEngine&) = delete;
    ~Win32SymbolEngine() {}

    static Win32SymbolEngine* sInstance;

    HANDLE m_processHandle;
    DWORD m_imageType;
    std::string m_processDir;
    std::string m_processName;

    LibDbgErr getSymbolModule(void* symAddress, SymbolInfo& symbolInfo);
    void walkThreadStack(HANDLE hThread, CONTEXT& context, StackFrameListener* listener);
};

extern LibDbgErr initWin32SymbolEngine();
extern LibDbgErr termWin32SymbolEngine();

}  // namespace libdbg

#endif  // DBGUTIL_WINDOWS

#endif  // __WIN32_SYM_HANDLER_H__