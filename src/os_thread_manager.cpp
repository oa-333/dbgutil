#include "os_thread_manager.h"

#include <atomic>
#include <cassert>
#include <thread>

#include "dbg_util_err.h"
#include "dbgutil_log_imp.h"
#include "os_thread_manager_internal.h"
#include "os_util.h"

namespace dbgutil {

static Logger sLogger;

static OsThreadManager* sThreadManager = nullptr;

void ThreadRequestFuture::release() { delete this; }

SignalRequest::SignalRequest(ThreadExecutor* executor, ThreadWaitMode waitMode,
                             uint64_t pollingIntervalMicros)
    : m_executor(executor),
      m_waitMode(waitMode),
      m_pollingIntervalMicros(pollingIntervalMicros),
      m_result(DBGUTIL_ERR_OK),
      m_flag(false) {}

void SignalRequest::notify(DbgUtilErr result) {
    if (m_waitMode == ThreadWaitMode::TWM_POLLING) {
        m_result.store(result, std::memory_order_relaxed);
        m_flag.store(true, std::memory_order_relaxed);
    } else {
        std::unique_lock<std::mutex> lock(m_lock);
        m_result.store(result, std::memory_order_relaxed);
        m_flag.store(true, std::memory_order_relaxed);
        m_cv.notify_one();
    }
}

DbgUtilErr SignalRequest::wait() {
    if (m_waitMode == ThreadWaitMode::TWM_POLLING) {
        while (m_flag.load(std::memory_order_relaxed) == false) {
            std::this_thread::sleep_for(std::chrono::microseconds(m_pollingIntervalMicros));
        }
    } else {
        std::unique_lock<std::mutex> lock(m_lock);
        m_cv.wait(lock, [this]() { return m_flag.load(std::memory_order_relaxed); });
    }
    return m_result.load(std::memory_order_relaxed);
}

void SignalRequest::exec() {
    DbgUtilErr result = m_executor->execRequest();
    notify(result);
}

DbgUtilErr OsThreadManager::execThreadRequest(
    os_thread_id_t threadId, ThreadExecutor* executor, DbgUtilErr& requestResult,
    const ThreadWaitParams& waitParams /* = ThreadWaitParams() */) {
    // if requesting to execute on current thread then we don't need to send a signal
    if (threadId == OsUtil::getCurrentThreadId()) {
        requestResult = executor->execRequest();
        return DBGUTIL_ERR_OK;
    }

    SignalRequest request(executor, waitParams.m_waitMode, waitParams.m_pollingIntervalMicros);
    DbgUtilErr rc = submitThreadSignalRequest(threadId, &request);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to send exec-request signal to thread %" PRItid, threadId);
        return rc;
    }

    // if a notifier was provided then use it now and wake up target thread once
    if (waitParams.m_notifier != nullptr) {
        waitParams.m_notifier->notify();
    }

    // wait for request to finish executing
    LOG_DEBUG(sLogger, "Signal SENT, waiting for signal handler to finish executing");
    requestResult = request.wait();
    LOG_DEBUG(sLogger, "Waiting DONE with result: %s", errorToString(requestResult));

    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsThreadManager::submitThreadRequest(
    os_thread_id_t threadId, ThreadExecutor* executor, ThreadRequestFuture*& future,
    const ThreadWaitParams& waitParams /* = ThreadWaitParams() */) {
    SignalRequest* request = new (std::nothrow)
        SignalRequest(executor, waitParams.m_waitMode, waitParams.m_pollingIntervalMicros);
    if (request == nullptr) {
        LOG_ERROR(sLogger,
                  "Cannot submit thread request, failed to allocate request object, out of memory");
        return DBGUTIL_ERR_NOMEM;
    }

    // if requesting to execute on current thread then we don't need to send a signal
    if (threadId == OsUtil::getCurrentThreadId()) {
        DbgUtilErr res = executor->execRequest();
        request->notify(res);
        return DBGUTIL_ERR_OK;
    }

    // submit request
    DbgUtilErr rc = submitThreadSignalRequest(threadId, request);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to send exec-request signal to thread %" PRItid, threadId);
        return rc;
    }

    return DBGUTIL_ERR_OK;
}

void setThreadManager(OsThreadManager* threadManager) {
    assert((threadManager != nullptr && sThreadManager == nullptr) ||
           (threadManager == nullptr && sThreadManager != nullptr));
    sThreadManager = threadManager;
    if (sThreadManager != nullptr) {
        registerLogger(sLogger, "os_thread_manager");
    } else {
        unregisterLogger(sLogger);
    }
}

OsThreadManager* getThreadManager() {
    assert(sThreadManager != nullptr);
    return sThreadManager;
}

}  // namespace dbgutil
