#ifndef __LIFE_SIGN_MANAGER_INTERNAL_H__
#define __LIFE_SIGN_MANAGER_INTERNAL_H__

#include <list>
#include <string>

#include "dbg_util_def.h"
#include "dbg_util_err.h"

namespace dbgutil {

/** @brief Retrieves the installed life-sign manager. */
extern void setLifeSignManager(LifeSignManager* lifeSignManager);

}  // namespace dbgutil

#endif  // __LIFE_SIGN_MANAGER_INTERNAL_H__