#include "os_stack_trace.h"

#include <cassert>

#include "os_stack_trace_internal.h"

namespace dbgutil {

static OsStackTraceProvider* sProvider = nullptr;

DbgUtilErr OsStackTraceProvider::getStackTrace(void* context, RawStackTrace& stackTrace) {
    struct StackFrameCollector : public StackFrameListener {
        StackFrameCollector(RawStackTrace& stackTrace) : m_stackTrace(stackTrace) {}
        StackFrameCollector(const StackFrameCollector&) = delete;
        StackFrameCollector(StackFrameCollector&&) = delete;
        StackFrameCollector& operator=(const StackFrameCollector&) = delete;
        ~StackFrameCollector() final {}

        void onStackFrame(void* frameAddress) final { m_stackTrace.push_back(frameAddress); }

        RawStackTrace& m_stackTrace;
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
