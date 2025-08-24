#ifndef __OS_THREAD_MANAGER_INTERNAL_H__
#define __OS_THREAD_MANAGER_INTERNAL_H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "os_thread_manager.h"

namespace dbgutil {

class DBGUTIL_API SignalRequest : public ThreadRequestFuture {
public:
    SignalRequest(ThreadExecutor* executor, ThreadWaitMode waitMode,
                  uint64_t pollingIntervalMicros);
    SignalRequest(const SignalRequest&) = delete;
    SignalRequest(SignalRequest&&) = delete;
    SignalRequest& operator=(const SignalRequest&) = delete;
    ~SignalRequest() override {}

    /** @brief Notifies asynchronous call ended with given result. */
    void notify(DbgUtilErr result);

    /** @brief Waits for the asynchronous thread request to finish, and returns its result. */
    DbgUtilErr wait() override;

    /** @brief Executes the request and notifies it is done. */
    void exec();

private:
    ThreadExecutor* m_executor;
    ThreadWaitMode m_waitMode;
    uint64_t m_pollingIntervalMicros;
    std::atomic<DbgUtilErr> m_result;
    std::mutex m_lock;
    std::condition_variable m_cv;
    std::atomic<bool> m_flag;
};

/** @brief Installs a thread manager. */
extern void setThreadManager(OsThreadManager* threadManager);

/** @brief Submits a request to execute on a target thread context by sending a signal. */
extern DbgUtilErr submitThreadSignalRequest(os_thread_id_t osThreadId, SignalRequest* request);

}  // namespace dbgutil

#endif  // __OS_THREAD_MANAGER_INTERNAL_H__