#ifndef __OS_STACK_TRACE_INTERNAL_H__
#define __OS_STACK_TRACE_INTERNAL_H__

#include "os_stack_trace.h"

namespace dbgutil {

/** @brief Installs a stack trace provider. */
extern void setStackTraceProvider(OsStackTraceProvider* provider);

}  // namespace dbgutil

#endif  // __OS_STACK_TRACE_INTERNAL_H__