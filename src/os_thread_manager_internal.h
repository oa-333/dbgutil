#ifndef __OS_THREAD_MANAGER_INTERNAL_H__
#define __OS_THREAD_MANAGER_INTERNAL_H__

#include "os_thread_manager.h"

namespace dbgutil {

/** @brief Installs a thread manager. */
extern void setThreadManager(OsThreadManager* threadManager);

}  // namespace dbgutil

#endif  // __OS_THREAD_MANAGER_INTERNAL_H__