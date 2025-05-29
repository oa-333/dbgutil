#ifndef __WIN32_EXCEPTION_HANDLER_H__
#define __WIN32_EXCEPTION_HANDLER_H__

#include "dbg_util_def.h"

namespace dbgutil {

extern DbgUtilErr initWin32ExceptionHandler();

extern DbgUtilErr termWin32ExceptionHandler();

}  // namespace dbgutil

#endif  // __WIN32_EXCEPTION_HANDLER_H__