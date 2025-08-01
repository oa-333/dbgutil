#ifndef __LINUX_EXCEPTION_HANDLER_H__
#define __LINUX_EXCEPTION_HANDLER_H__

#include "dbg_util_def.h"

#ifndef DBGUTIL_MSVC

#include <csignal>
#include <unordered_map>

#include "os_exception_handler.h"

namespace libdbg {

class LinuxExceptionHandler : public OsExceptionHandler {
public:
    /** @brief Creates the singleton instance of the exception handler for Linux platform. */
    static void createInstance();

    /** @brief Retrieves a reference to the single instance of the exception handler. */
    static LinuxExceptionHandler* getInstance();

    /** @brief Destroys the singleton instance of the exception handler. */
    static void destroyInstance();

protected:
    /** @brief Initializes the symbol engine. */
    LibDbgErr initializeEx() final;

    /** @brief Destroys the symbol engine. */
    LibDbgErr terminateEx() final;

private:
    LinuxExceptionHandler() {}
    ~LinuxExceptionHandler() final {}

    static LinuxExceptionHandler* sInstance;

#ifdef DBGUTIL_MINGW
    typedef __p_sig_fn_t SignalHandlerFunc;
    typedef __p_sig_fn_t SignalHandler;
#else
    typedef void (*SignalHandlerFunc)(int, siginfo_t*, void*);
    typedef struct sigaction SignalHandler;
#endif
    typedef std::unordered_map<int, SignalHandler> SigHandlerMap;
    SigHandlerMap m_prevHandlerMap;

#ifdef DBGUTIL_MINGW
    static void signalHandlerStatic(int sigNum);
    void signalHandler(int sigNum);
#else
    static void signalHandlerStatic(int sigNum, siginfo_t* sigInfo, void* context);
    void signalHandler(int sigNum, siginfo_t* sigInfo, void* context);
#endif

    const char* getSignalName(int sigNum);

    LibDbgErr registerExceptionHandlers();
    LibDbgErr unregisterExceptionHandlers();
    LibDbgErr registerSignalHandler(int sigNum);
    LibDbgErr unregisterSignalHandler(int sigNum);

    LibDbgErr registerSignalHandler(int sigNum, SignalHandlerFunc handler,
                                    SignalHandler* prevHandler);
    LibDbgErr restoreSignalHandler(int sigNum, SignalHandler& handler);

    void finalizeSignalHandling(OsExceptionInfo& exInfo, void* context);
};

extern LibDbgErr initLinuxExceptionHandler();

extern LibDbgErr termLinuxExceptionHandler();

}  // namespace libdbg

#endif  // not defined DBGUTIL_MSVC

#endif  // __LINUX_EXCEPTION_HANDLER_H__