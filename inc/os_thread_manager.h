#ifndef __OS_THREAD_MANGER_H__
#define __OS_THREAD_MANGER_H__

#include <condition_variable>
#include <cstdint>

#include "dbg_util_def.h"
#include "dbg_util_err.h"

namespace dbgutil {

/** @brief A thread visitor used to traverse all threads. */
class DBGUTIL_API ThreadVisitor {
public:
    virtual ~ThreadVisitor() {}

    /**
     * @brief Handles a visited thread.
     * @param threadId The system thread id.
     */
    virtual void onThreadId(os_thread_id_t threadId) = 0;

protected:
    ThreadVisitor() {}
    ThreadVisitor(const ThreadVisitor&) = delete;
    ThreadVisitor(ThreadVisitor&&) = delete;
    ThreadVisitor& operator=(const ThreadVisitor&) = delete;
};

/** @brief An active executor used for executing some operation on a target thread context. */
class DBGUTIL_API ThreadExecutor {
public:
    virtual ~ThreadExecutor() {}

    /** @brief Executes an operation on a target thread context. */
    virtual DbgUtilErr execRequest() = 0;

protected:
    ThreadExecutor() {}
    ThreadExecutor(const ThreadExecutor&) = delete;
    ThreadExecutor(ThreadExecutor&&) = delete;
    ThreadExecutor& operator=(const ThreadExecutor&) = delete;
};

/** @brief Wait mode constants. */
enum class ThreadWaitMode : uint32_t {
    /** @var Designates polling wait mode. */
    TWM_POLLING,

    /** @var Designates wait by notification mode (i.e. condition variable). */
    TWM_NOTIFY
};

/** @brief Thread notifier required for sending thread signals. */
class DBGUTIL_API ThreadNotifier {
public:
    virtual ~ThreadNotifier() {}

    /** @brief Notifies the target thread request to wake up and consume pending signals. */
    virtual void notify() = 0;

protected:
    ThreadNotifier() {}
    ThreadNotifier(const ThreadNotifier&) = delete;
    ThreadNotifier(ThreadNotifier&&) = delete;
    ThreadNotifier& operator=(const ThreadNotifier&) = delete;
};

/** @brief Thread notifier for condition variable. */
class DBGUTIL_API CVThreadNotifier : public ThreadNotifier {
public:
    CVThreadNotifier() = delete;
    CVThreadNotifier(std::condition_variable& cv) : m_cv(cv) {}
    CVThreadNotifier(const CVThreadNotifier&) = delete;
    CVThreadNotifier(CVThreadNotifier&&) = delete;
    CVThreadNotifier& operator=(const CVThreadNotifier&) = delete;
    ~CVThreadNotifier() override {}

    void notify() override { m_cv.notify_one(); }

private:
    std::condition_variable& m_cv;
};

/** @struct Thread wait parameters. */
struct DBGUTIL_API ThreadWaitParams {
    ThreadWaitParams(ThreadWaitMode waitMode = ThreadWaitMode::TWM_POLLING,
                     uint64_t pollingIntervalMicros = 0, ThreadNotifier* notifier = nullptr)
        : m_waitMode(waitMode),
          m_pollingIntervalMicros(pollingIntervalMicros),
          m_notifier(notifier) {}
    ThreadWaitParams(const ThreadWaitParams&) = default;
    ThreadWaitParams(ThreadWaitParams&&) = delete;
    ThreadWaitParams& operator=(const ThreadWaitParams&) = default;
    ~ThreadWaitParams() {}

    /** @brief The wait mode, either polling or notify (by default polling mode). */
    ThreadWaitMode m_waitMode;

    /**
     * @brief In case polling wait mode is used, this specifies the polling interval in
     * microseconds. If zero is passed then a tight loop will be used.
     */
    uint64_t m_pollingIntervalMicros;

    /**
     * @brief Optional notifier, required for waking up the target thread and consume incoming
     * signals.
     */
    ThreadNotifier* m_notifier;
};

/**
 * @brief A future interface for waiting on and collecting results of asynchronous thread requests.
 */
class DBGUTIL_API ThreadRequestFuture {
public:
    /** @brief Waits for the asynchronous thread request to finish, and returns its result. */
    virtual DbgUtilErr wait() = 0;

    /** @brief Deallocates the future object when done (required for preventing heap mismatch). */
    void release();

protected:
    ThreadRequestFuture() {}
    ThreadRequestFuture(const ThreadRequestFuture&) = delete;
    ThreadRequestFuture(ThreadRequestFuture&&) = delete;
    ThreadRequestFuture& operator=(const ThreadRequestFuture&) = delete;
    virtual ~ThreadRequestFuture() {}
};

class DBGUTIL_API OsThreadManager {
public:
    OsThreadManager(const OsThreadManager&) = delete;
    OsThreadManager(OsThreadManager&&) = delete;
    OsThreadManager& operator=(const OsThreadManager&) = delete;
    virtual ~OsThreadManager() {}

    /**
     * @brief Traverses all running thread ids.
     * @param visitor The thread visitor.
     * @return The operation result.
     */
    virtual DbgUtilErr visitThreadIds(ThreadVisitor* visitor) = 0;

    /**
     * @brief Requests to execute an operation on another thread (blocking call).
     * @note On Windows platforms, this call is susceptible to dead-lock, since the target thread
     * may not receive the signal (sent as APC request). For this reason it is recommended to either
     * call instead @ref submitThreadRequest(), or provide a @ref ThreadNotifier through the wait
     * parameters and implement the logic required for waking up the target thread. Especially this
     * is true when the target thread is not in alert-able wait state, such as waiting on
     * std::condition_variable. In the latter case, it suffices to call notify_one() on the
     * condition variable associated with the target thread before waiting on the future object.
     * Failing to do so will most certainly result in a deadlock.
     *
     * @param threadId The destination thread id.
     * @param executor The request executor.
     * @param[out] requestResult The request execution result (valid only if true is passed for the
     * wait parameter, and the call to @ref execThreadRequest() succeeded).
     * @param waitParams Optionally specifies the wait parameters.
     * @return DbgUtilErr The operation result. This refers only to the ability to post the
     * operation to be executed on the target thread, and then collecting the result. The actual
     * result of the operation being executed on the target thread, is returned via the @ref
     * opResult out parameter.
     */
    DbgUtilErr execThreadRequest(os_thread_id_t threadId, ThreadExecutor* executor,
                                 DbgUtilErr& requestResult,
                                 const ThreadWaitParams& waitParams = ThreadWaitParams());

    /**
     * @brief Submits a request to execute an operation on another thread (non-blocking call). A
     * future object is returned for waiting on the request to finish.
     *
     * @note On Windows platforms, in order to avoid possible dead-locks it is recommended to notify
     * the target thread to wake up (and probably go to sleep again) BEFORE waiting on the future
     * object. Especially this is true when the target thread is not in alert-able wait state, such
     * as waiting on std::condition_variable. In the latter case, it suffices to call notify_one()
     * on the condition variable associated with the target thread before waiting on the future
     * object. Failing to do so will most certainly result in a deadlock.
     *
     * @param threadId The destination thread id.
     * @param executor The request executor.
     * @param[out] future Receives a pointer to a future object (valid only when request submission
     * succeeds). The future object must be waited upon and released when done.
     * @param waitParams Optionally specifies the wait parameters. The notifier member is ignored.
     * @return DbgUtilErr The operation result. This refers only to the ability to post the
     * operation to be executed on the target thread. The actual operation's result will be
     * available through the future object when the operation is done.
     */
    DbgUtilErr submitThreadRequest(os_thread_id_t threadId, ThreadExecutor* executor,
                                   ThreadRequestFuture*& future,
                                   const ThreadWaitParams& waitParams = ThreadWaitParams());

protected:
    OsThreadManager() {}
};

/** @brief Retrieves the installed thread manager. */
extern DBGUTIL_API OsThreadManager* getThreadManager();

/** @brief Utility API for lambda syntax. */
template <typename F>
inline void visitThreadIds(F f) {
    struct Visitor final : public ThreadVisitor {
        Visitor(F f) : m_f(f) {}
        void onThread(os_thread_id_t threadId) { m_f(threadId); }
        F m_f;
    };
    Visitor visitor(f);
    getThreadManager()->visitThreadIds(&visitor);
}

/** @brief Utility API for lambda syntax. */
template <typename F>
inline DbgUtilErr execThreadRequest(os_thread_id_t threadId, DbgUtilErr& requestResult,
                                    const ThreadWaitParams& waitParams, F f) {
    struct Executor final : public ThreadExecutor {
        Executor(F f) : m_f(f) {}
        Executor() = delete;
        Executor(const Executor&) = delete;
        Executor(Executor&&) = delete;
        Executor& operator=(const Executor&) = delete;
        ~Executor() final {}
        DbgUtilErr execRequest() final { return m_f(); }
        F m_f;
    };
    Executor executor(f);
    return getThreadManager()->execThreadRequest(threadId, &executor, requestResult, waitParams);
}

/** @brief Utility API for lambda syntax. */
template <typename F>
inline DbgUtilErr submitThreadRequest(os_thread_id_t threadId, ThreadRequestFuture*& future,
                                      const ThreadWaitParams& waitParams, F f) {
    struct Executor final : public ThreadExecutor {
        Executor(F f) : m_f(f) {}
        Executor() = delete;
        Executor(const Executor&) = delete;
        Executor(Executor&&) = delete;
        ~Executor() final {}
        Executor& operator=(const Executor&) = delete;
        DbgUtilErr execRequest() final { return m_f(); }
        F m_f;
    };
    Executor executor(f);
    return getThreadManager()->submitThreadRequest(threadId, &executor, future, waitParams);
}

}  // namespace dbgutil

#endif  // __OS_THREAD_MANGER_H__