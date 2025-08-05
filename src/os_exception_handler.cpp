#include "os_exception_handler.h"

#include <cassert>

#include "dbg_stack_trace.h"
#include "dbg_util_flags.h"
#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "os_module_manager_internal.h"

namespace dbgutil {

static Logger sLogger;

// thread local static buffer for call stack (avoid allocation during panic)
#define CALL_STACK_BUF_SIZE 8192
static thread_local char sCallStackBuf[CALL_STACK_BUF_SIZE];
static size_t sCallStackBufLen = 0;

static OsExceptionHandler* sExceptionHandler = nullptr;

class CallStackFilter : public StackEntryFilter {
public:
    CallStackFilter() {}
    CallStackFilter(const CallStackFilter&) = delete;
    CallStackFilter(CallStackFilter&&) = delete;
    CallStackFilter& operator=(const CallStackFilter&) = delete;
    ~CallStackFilter() final {}

    /**
     * @brief Filters a stack trace entry
     * @param stackEntry The stack entry.
     * @return true if the stack entry is to be processed, or false if should be skipped.
     */
    bool filterStackEntry(const StackEntry& stackEntry) final {
        // discard dbgutil frames
        return stackEntry.m_entryInfo.m_moduleBaseAddress != getSelfLoadAddress();
    }
};

/** @brief Stack entry printer to string. */
class CallStackBufPrinter : public StackEntryPrinter {
public:
    CallStackBufPrinter() {}
    CallStackBufPrinter(const CallStackBufPrinter&) = delete;
    CallStackBufPrinter(CallStackBufPrinter&&) = delete;
    CallStackBufPrinter& operator=(const CallStackBufPrinter&) = delete;
    ~CallStackBufPrinter() final {}

    void onBeginStackTrace(os_thread_id_t threadId) override {
        sCallStackBuf[0] = 0;
        int res = snprintf(sCallStackBuf + sCallStackBufLen, CALL_STACK_BUF_SIZE - sCallStackBufLen,
                           "[Thread %" PRItidx " stack trace]\n", threadId);
        if (res > 0) {
            sCallStackBufLen += (size_t)res;
        }
    }
    void onEndStackTrace() override {}
    void onStackEntry(const char* stackEntry) {
        int res = snprintf(sCallStackBuf + sCallStackBufLen, CALL_STACK_BUF_SIZE - sCallStackBufLen,
                           "%s\n", stackEntry);
        if (res > 0) {
            sCallStackBufLen += (size_t)res;
        }
    }
};

DbgUtilErr OsExceptionHandler::initialize() {
    registerLogger(sLogger, "os_exception_handler");
    setTerminateHandler();
    DbgUtilErr res = initializeEx();
    if (res != DBGUTIL_ERR_OK) {
        restoreTerminateHandler();
        unregisterLogger(sLogger);
    }
    return res;
}

DbgUtilErr OsExceptionHandler::terminate() {
    DbgUtilErr res = terminateEx();
    if (res != DBGUTIL_ERR_OK) {
        return res;
    }
    restoreTerminateHandler();
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

void OsExceptionHandler::dispatchExceptionInfo(const OsExceptionInfo& exceptionInfo) {
    if (m_exceptionListener != nullptr) {
        m_exceptionListener->onException(exceptionInfo);
    }
}

const char* OsExceptionHandler::prepareCallStack(void* context) {
    // get stack trace information
    CallStackFilter filter;
    CallStackBufPrinter callStackBufPrinter;
    printStackTraceContext(context, 0, &filter, nullptr, &callStackBufPrinter);
    return sCallStackBuf;
}

void OsExceptionHandler::setTerminateHandler() {
    if (getGlobalFlags() & DBGUTIL_SET_TERMINATE_HANDLER) {
        m_prevTerminateHandler = std::get_terminate();
        std::set_terminate(&OsExceptionHandler::terminateHandler);
    }
}

void OsExceptionHandler::restoreTerminateHandler() {
    if (getGlobalFlags() & DBGUTIL_SET_TERMINATE_HANDLER) {
        std::set_terminate(m_prevTerminateHandler);
    }
}

void OsExceptionHandler::terminateHandler() noexcept {
    // delegate to non-static class member
    OsExceptionHandler* exceptionHandler = getExceptionHandler();
    exceptionHandler->handleTerminate();
}

void OsExceptionHandler::handleTerminate() {
    // prepare call stack first
    CallStackFilter filter;
    CallStackBufPrinter callStackBufPrinter;
    printStackTraceContext(nullptr, 0, &filter, nullptr, &callStackBufPrinter);

    // dispatch to exception listener
    if (m_exceptionListener != nullptr) {
        m_exceptionListener->onTerminate(sCallStackBuf);
    }

    // send to log
    if (getGlobalFlags() & DBGUTIL_LOG_EXCEPTIONS) {
        LOG_FATAL(sLogger, "std::terminate() called, call stack information:\n\n%s\n",
                  sCallStackBuf);
    }

    // delegate to another handler or abort
    if (m_prevTerminateHandler != nullptr) {
        // let previous handler installed by user to decide what to do
        m_prevTerminateHandler();
    } else {
        // if no previous handler installed by the user, then call abort()
        abort();
    }
}

void setExceptionHandler(OsExceptionHandler* symbolEngine) {
    assert((symbolEngine != nullptr && sExceptionHandler == nullptr) ||
           (symbolEngine == nullptr && sExceptionHandler != nullptr));
    sExceptionHandler = symbolEngine;
}

OsExceptionHandler* getExceptionHandler() {
    assert(sExceptionHandler != nullptr);
    return sExceptionHandler;
}

}  // namespace dbgutil
