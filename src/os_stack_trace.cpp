#include "os_stack_trace.h"

#include <cassert>

namespace dbgutil {

static OsStackTraceProvider* sProvider = nullptr;

DbgUtilErr OsStackTraceProvider::getStackTrace(void* context, RawStackTrace& stackTrace) {
    struct StackFrameCollector : public StackFrameListener {
        RawStackTrace& m_stackTrace;
        StackFrameCollector(RawStackTrace& stackTrace) : m_stackTrace(stackTrace) {}
        void onStackFrame(void* frameAddress) final { m_stackTrace.push_back(frameAddress); }
    };

    StackFrameCollector collector(stackTrace);
    return walkStack(&collector, context);
}

void setStackTraceProvider(OsStackTraceProvider* provider) {
    assert((provider != nullptr && sProvider == nullptr) ||
           (provider == nullptr && sProvider != nullptr));
    sProvider = provider;
}

OsStackTraceProvider* getStackTraceProvider() {
    assert(sProvider != nullptr);
    return sProvider;
}

}  // namespace dbgutil
