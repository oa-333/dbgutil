#ifndef __WIN32_EXCEPTION_HANDLER_H__
#define __WIN32_EXCEPTION_HANDLER_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

#include "os_exception_handler.h"

namespace libdbg {

class Win32ExceptionHandler : public OsExceptionHandler {
public:
    /** @brief Creates the singleton instance of the exception handler for Windows platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the exception handler. */
    static Win32ExceptionHandler* getInstance();

    /** @brief Destroys the singleton instance of the exception handler. */
    static void destroyInstance();

protected:
    /** @brief Initializes the symbol engine. */
    DbgUtilErr initializeEx() final;

    /** @brief Destroys the symbol engine. */
    DbgUtilErr terminateEx() final;

private:
    Win32ExceptionHandler() {}
    Win32ExceptionHandler(const Win32ExceptionHandler&) = delete;
    Win32ExceptionHandler(Win32ExceptionHandler&&) = delete;
    Win32ExceptionHandler& operator=(const Win32ExceptionHandler&) = delete;
    ~Win32ExceptionHandler() final {}

    static Win32ExceptionHandler* sInstance;
    static LPTOP_LEVEL_EXCEPTION_FILTER sPrevFilter;

    void registerExceptionHandler();
    void unregisterExceptionHandler();

    static LONG WINAPI unhandledExceptionFilterStatic(_EXCEPTION_POINTERS* exceptionInfo) noexcept;
    void unhandledExceptionFilter(_EXCEPTION_POINTERS* exceptionInfo);
};

extern DbgUtilErr initWin32ExceptionHandler();

extern DbgUtilErr termWin32ExceptionHandler();

}  // namespace libdbg

#endif  // DBGUTIL_WINDOWS

#endif  // __WIN32_EXCEPTION_HANDLER_H__