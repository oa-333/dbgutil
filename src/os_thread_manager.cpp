#include "os_thread_manager.h"

#include <cassert>

namespace dbgutil {

static OsThreadManager* sThreadManager = nullptr;

void setThreadManager(OsThreadManager* threadManager) {
    assert((threadManager != nullptr && sThreadManager == nullptr) ||
           (threadManager == nullptr && sThreadManager != nullptr));
    sThreadManager = threadManager;
}

OsThreadManager* getThreadManager() {
    assert(sThreadManager != nullptr);
    return sThreadManager;
}

}  // namespace dbgutil
