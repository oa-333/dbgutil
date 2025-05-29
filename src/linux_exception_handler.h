#ifndef __LINUX_EXCEPTION_HANDLER_H__
#define __LINUX_EXCEPTION_HANDLER_H__

#include "dbg_util_def.h"

namespace dbgutil {

extern DbgUtilErr initLinuxExceptionHandler();

extern DbgUtilErr termLinuxExceptionHandler();

}  // namespace dbgutil

#endif  // __LINUX_EXCEPTION_HANDLER_H__