#ifndef __DBG_UTIL_EXCEPT_H__
#define __DBG_UTIL_EXCEPT_H__

#include <cinttypes>

#include "dbg_util_def.h"

namespace dbgutil {

#ifdef DBGUTIL_WINDOWS
/**
 * @typedef Exception code type as defined by the ExceptionCode member of EXCEPTION_RECORD.
 * @note This is actually DWORD, but we don't want to force entire windows header inclusion here
 * just for the sake of one DWORD type definition.
 */
typedef unsigned long exception_code_t;
#else
/** @typedef Exception code type as defined in signal header. */
typedef int exception_code_t;
#endif

/** @brief Exception information. */
struct DBGUTIL_API OsExceptionInfo {
    /** @brief The exception code (e.g. SIGSEGV, STATUS_ACCESS_VIOLATION). */
    exception_code_t m_exceptionCode;

    /** @brief A possible sub-exception code (e.g. FPE_INTDIV). */
    exception_code_t m_exceptionSubCode;

    /** @brief The faulting address (i.e. address of the instruction causing the exception). */
    void* m_faultAddress;

    /** @brief The name of the exception in human readable form. */
    const char* m_exceptionName;

    /** @brief A full, formatted, exception information. */
    const char* m_fullExceptionInfo;

    /** @brief A full, resolved and formatted call stack of the exception. */
    const char* m_callStack;
};

/** @brief Exception listener. */
class DBGUTIL_API OsExceptionListener {
public:
    virtual ~OsExceptionListener() {}

    /** @brief Handle exception (e.g. SIGSEGV, STATUS_ACCESS_VIOLATION). */
    virtual void onException(const OsExceptionInfo& exceptionInfo) = 0;

    /** @brief Handle std::terminate() being called. */
    virtual void onTerminate(const char* callStack) = 0;

protected:
    OsExceptionListener() {}
    OsExceptionListener(const OsExceptionListener&) = delete;
    OsExceptionListener(OsExceptionListener&&) = delete;
};

}  // namespace dbgutil

#endif  // __DBG_UTIL_EXCEPT_H__