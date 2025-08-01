#ifndef __OS_EXCEPTION_HANDLER_H__
#define __OS_EXCEPTION_HANDLER_H__

#include <exception>

#include "dbg_util_def.h"
#include "dbg_util_err.h"
#include "dbg_util_except.h"

namespace libdbg {

/** @brief Parent interface for exception handler. */
class DBGUTIL_API OsExceptionHandler {
public:
    OsExceptionHandler(const OsExceptionHandler&) = delete;
    OsExceptionHandler(OsExceptionHandler&&) = delete;
    OsExceptionHandler& operator=(const OsExceptionHandler&) = delete;
    virtual ~OsExceptionHandler() {}

    /** @brief Initializes the symbol engine. */
    DbgUtilErr initialize();

    /** @brief Destroys the symbol engine. */
    DbgUtilErr terminate();

    /**
     * @brief Retrieves symbol debug information (platform independent API).
     * @param symAddress The symbol address.
     * @param[out] symbolInfo The symbol information.
     */
    inline void setExceptionListener(OsExceptionListener* exceptionListener) {
        m_exceptionListener = exceptionListener;
    }

protected:
    OsExceptionHandler() : m_exceptionListener(nullptr), m_prevTerminateHandler(nullptr) {}

    /** @brief Initializes the symbol engine. */
    virtual DbgUtilErr initializeEx() { return DBGUTIL_ERR_OK; }

    /** @brief Destroys the symbol engine. */
    virtual DbgUtilErr terminateEx() { return DBGUTIL_ERR_OK; }

    /** @brief Dispatches an exception to the exception listener. */
    void dispatchExceptionInfo(const OsExceptionInfo& exceptionInfo);

    /** @brief Prepares a call stack (for exception info preparation). */
    const char* prepareCallStack(void* context);

private:
    OsExceptionListener* m_exceptionListener;
    std::terminate_handler m_prevTerminateHandler;

    void setTerminateHandler();
    void restoreTerminateHandler();
    static void terminateHandler() noexcept;

    void handleTerminate();
};

/** @brief Installs a exception handler implementation. */
extern DBGUTIL_API void setExceptionHandler(OsExceptionHandler* symbolEngine);

/** @brief Retrieves the installed exception handler implementation. */
extern DBGUTIL_API OsExceptionHandler* getExceptionHandler();

}  // namespace libdbg

#endif  // __OS_EXCEPTION_HANDLER_H__