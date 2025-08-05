#ifndef __OS_EXCEPTION_HANDLER_INTERNAL_H__
#define __OS_EXCEPTION_HANDLER_INTERNAL_H__

#include "os_exception_handler.h"

namespace dbgutil {

/** @brief Installs a exception handler implementation. */
extern void setExceptionHandler(OsExceptionHandler* exceptionHandler);

}  // namespace dbgutil

#endif  // __OS_EXCEPTION_HANDLER_INTERNAL_H__