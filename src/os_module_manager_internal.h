#ifndef __OS_MODULE_MANAGER_INTERNAL_H__
#define __OS_MODULE_MANAGER_INTERNAL_H__

#include "os_module_manager.h"

namespace dbgutil {

/** @brief Installs a module manager implementation. */
extern void setModuleManager(OsModuleManager* moduleManager);

/** @brief Retrieves the load address of the dbgutil module. */
extern void* getSelfLoadAddress();

}  // namespace dbgutil

#endif  // __OS_MODULE_MANAGER_INTERNAL_H__