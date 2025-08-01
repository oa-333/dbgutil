#include "libdbg_def.h"

#ifdef LIBDBG_GCC

#ifdef LIBDBG_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <pthread.h>
#include <signal.h>

#include <atomic>
#include <cassert>
#include <cinttypes>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "dir_scanner.h"
#include "libdbg_common.h"
#include "libdbg_log_imp.h"
#include "linux_thread_manager.h"
#include "os_util.h"

#ifdef LIBDBG_MINGW
#include "win32_thread_manager.h"
#endif

// headers required for dir/file API
#ifndef LIBDBG_MINGW
#include <sys/syscall.h>
#ifdef SYS_rt_tgsigqueueinfo
#define rt_tgsigqueueinfo(tgid, tid, sig, info) syscall(SYS_rt_tgsigqueueinfo, tgid, tid, sig, info)
#else
#error "SYS_rt_sigqueueinfo unavailable on this platform"
#endif
#endif

// currently polling tightly
#define DBGUTIL_REQUEST_POLL_FREQ_MILLIS 0

#ifndef LIBDBG_MINGW
#define SIG_EXEC_REQUEST (SIGRTMIN + 1)
#endif

// Design Notes
// ============
// Instead of having a global map with locks, which is not safe to access from a signal handler, the
// choice has been made to use rt_tgsigqueueinfo() system call, which unlike pthread_kill(), offers
// both the ability to send signal to a specific thread and with an extra parameter.
// This way, a map is not needed at all. Every time call stack or some other request needs to be
// executed on a target thread, we simply send an executable object to the thread via
// rt_tgsigqueueinfo(), and voila. No async-signal safety issues, no global map, no locks, no
// condition variables, but with one small downside, it requires tight loop for result collecting.
// This can be adjusted with some polling mechanism. For now a tight loop is used with yield().

namespace libdbg {

static Logger sLogger;

// static thread_local bool isHandlingSignal = false;

class SignalRequest {
protected:
    SignalRequest() : m_flag(false) {}

    void notify() { m_flag.store(true, std::memory_order_relaxed); }

    virtual void execImpl() = 0;

public:
    void wait(uint32_t pollMillis) {
        while (m_flag.load(std::memory_order_relaxed) == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(pollMillis));
        }
    }

    void exec() {
        execImpl();
        notify();
    }

private:
    std::atomic<bool> m_flag;
};

class ExternRequest : public SignalRequest {
public:
    ExternRequest(ThreadExecutor* executor = nullptr)
        : m_executor(executor), m_result(LIBDBG_ERR_OK) {}

    inline LibDbgErr getResult() const { return m_result; }

protected:
    void execImpl() { m_result = m_executor->execRequest(); }

private:
    ThreadExecutor* m_executor;
    LibDbgErr m_result;
};

/*static void verifyAsyncSignalSafe() {
    if (isHandlingSignal) {
        fprintf(stderr, "*** VIOLATING ASYNC-SIGNAL SAFETY ***\n");
    }
}*/

#ifdef LIBDBG_MINGW
static void apcRoutine(ULONG_PTR data) {
    LOG_DEBUG(sLogger, "Received APC");
    SignalRequest* request = (SignalRequest*)(void*)data;
    request->exec();
}
#endif

#ifdef LIBDBG_MINGW
static LibDbgErr execThreadSignalRequest(os_thread_id_t osThreadId, SignalRequest* request) {
    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, osThreadId);
    if (hThread == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, OpenThread, "Failed to get thread %" PRItid " handle", osThreadId);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }

    // if (!QueueUserAPC2(apcRoutine, hThread, NULL, QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC)) {
    if (!QueueUserAPC(apcRoutine, hThread, (ULONG_PTR)request)) {
        LOG_WIN32_ERROR(sLogger, QueueUserAPC, "Failed to queue user APC to thread %" PRItid,
                        osThreadId);
        CloseHandle(hThread);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    CloseHandle(hThread);
    return LIBDBG_ERR_OK;
}
#else
static LibDbgErr execThreadSignalRequest(os_thread_id_t osThreadId, SignalRequest* request) {
    siginfo_t si;
    si.si_code = SI_QUEUE;
    si.si_pid = getpid();
    si.si_uid = getuid();
    si.si_value.sival_ptr = request;
    if (rt_tgsigqueueinfo(getpid(), osThreadId, SIG_EXEC_REQUEST, &si) == -1) {
        LOG_SYS_ERROR(sLogger, rt_sigqueueinfo, "Failed to send signal to thread %" PRItid,
                      osThreadId);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
}
#endif

LinuxThreadManager* LinuxThreadManager::sInstance = nullptr;

#ifdef LIBDBG_LINUX
static void signalHandler(int sigNum, siginfo_t* sigInfo, void* context) {
    // isHandlingSignal = true;
    LOG_DEBUG(sLogger, "Received signal: %s (%d)", strsignal(sigNum), sigNum);
    SignalRequest* request = (SignalRequest*)sigInfo->si_value.sival_ptr;
    request->exec();
    // isHandlingSignal = false;
}
#endif

#ifdef LIBDBG_LINUX
static LibDbgErr registerSignalHandler(int sigNum) {
    struct sigaction sa = {};
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    int res = sigaction(sigNum, &sa, nullptr);
    if (res != 0) {
        LOG_SYS_ERROR(sLogger, sigaction, "Failed to register signal handler");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    LOG_DEBUG(sLogger, "Registered signal %d handler", sigNum);
    return LIBDBG_ERR_OK;
}

static LibDbgErr unregisterSignalHandler(int sigNum) {
    struct sigaction sa = {};
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = nullptr;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    int res = sigaction(sigNum, &sa, nullptr);
    if (res != 0) {
        LOG_SYS_ERROR(sLogger, sigaction, "Failed to unregister signal handler");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    LOG_DEBUG(sLogger, "Unregistered signal %d handler", sigNum);
    return LIBDBG_ERR_OK;
}
#endif

void LinuxThreadManager::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) LinuxThreadManager();
    assert(sInstance != nullptr);
}

LinuxThreadManager* LinuxThreadManager::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void LinuxThreadManager::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

LibDbgErr LinuxThreadManager::initialize() {
#ifndef LIBDBG_MINGW
    return registerSignalHandler(SIG_EXEC_REQUEST);
#else
    // MinGW does not implement signals well, so we need a workaround using QueueUserAPC2
    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr LinuxThreadManager::terminate() {
#ifdef LIBDBG_LINUX
    return unregisterSignalHandler(SIG_EXEC_REQUEST);
#else
    return LIBDBG_ERR_OK;
#endif
}

class ThreadIdVisitor : public DirEntryVisitor {
public:
    ThreadIdVisitor(ThreadVisitor* visitor) : m_visitor(visitor) {}
    ~ThreadIdVisitor() final {}

    void onDirEntry(const DirEntryInfo& dirEntry) final {
        if (dirEntry.m_type == DirEntryType::DET_DIR) {
            os_thread_id_t osThreadId = 0;
            if (parseThreadId(dirEntry.m_name, osThreadId)) {
                m_visitor->onThreadId(osThreadId);
            }
        }
    }

private:
    ThreadVisitor* m_visitor;

    bool parseThreadId(const std::string& taskIdName, os_thread_id_t& osThreadId) {
        std::size_t pos = 0;
        try {
            osThreadId = std::stol(taskIdName, &pos);
        } catch (std::exception& e) {
            LOG_SYS_ERROR(sLogger, std::stoull,
                          "Failed to convert Linux task name %s to integer value: %s",
                          taskIdName.c_str(), e.what());
            return false;
        }

        if (pos != taskIdName.length()) {
            LOG_ERROR(sLogger,
                      "Excess characters at Linux task id name, integer conversion failed: %s",
                      taskIdName.c_str());
            return false;
        }
        return true;
    }
};

LibDbgErr LinuxThreadManager::visitThreadIds(ThreadVisitor* visitor) {
#ifdef LIBDBG_MINGW
    // divert to Win32
    return Win32ThreadManager::getInstance()->visitThreadIds(visitor);
#else
    // take a snapshot of all running threads through /proc/self/task
    ThreadIdVisitor dirVisitor(visitor);
    LibDbgErr rc = DirScanner::visitDirEntries("/proc/self/task", &dirVisitor);
    if (rc != LIBDBG_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to list directory entries under /proc/self/task: %s",
                  errorCodeToStr(rc));
        return rc;
    }

    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr LinuxThreadManager::getThreadHandle(os_thread_id_t threadId, pthread_t& threadHandle) {
    class GetThreadHandleExecutor : public ThreadExecutor {
    public:
        GetThreadHandleExecutor() {}
        ~GetThreadHandleExecutor() final {}

        LibDbgErr execRequest() final {
            m_threadHandle = pthread_self();
            return LIBDBG_ERR_OK;
        }

        inline os_thread_id_t getThreadHandle() const { return m_threadHandle; }

    private:
        pthread_t m_threadHandle;
    };

    GetThreadHandleExecutor requestExecutor;
    LibDbgErr result = LIBDBG_ERR_OK;
    LibDbgErr rc = execThreadRequest(threadId, &requestExecutor, result);
    if (rc == LIBDBG_ERR_OK) {
        rc = result;
        if (rc == LIBDBG_ERR_OK) {
            threadHandle = requestExecutor.getThreadHandle();
        }
    }
    return rc;
}

LibDbgErr LinuxThreadManager::execThreadRequest(os_thread_id_t threadId, ThreadExecutor* executor,
                                                LibDbgErr& requestResult) {
    // if requesting to execute on current thread then we don't need to send a signal
    if (threadId == OsUtil::getCurrentThreadId()) {
        requestResult = executor->execRequest();
        return LIBDBG_ERR_OK;
    }

    ExternRequest request(executor);
    LibDbgErr rc = execThreadSignalRequest(threadId, &request);
    if (rc != LIBDBG_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to send exec-request signal to thread %" PRItid, threadId);
        return rc;
    }

    // there is no escaping indefinite block until signal is served or error occurs
    // if we bail out too early, and suddenly the signal is delivered, then the signal handler will
    // access the thread map without lock-guard

    LOG_DEBUG(sLogger, "Signal SENT, waiting for signal handler to finish executing");
    request.wait(DBGUTIL_REQUEST_POLL_FREQ_MILLIS);
    requestResult = request.getResult();
    LOG_DEBUG(sLogger, "Waiting DONE with result: %s", errorCodeToStr(requestResult));

    return LIBDBG_ERR_OK;
}

LibDbgErr initLinuxThreadManager() {
    registerLogger(sLogger, "linux_thread_manager");
    LinuxThreadManager::createInstance();
    LibDbgErr rc = LinuxThreadManager::getInstance()->initialize();
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    setThreadManager(LinuxThreadManager::getInstance());
    return LIBDBG_ERR_OK;
}

LibDbgErr termLinuxThreadManager() {
    setThreadManager(nullptr);
    LibDbgErr rc = LinuxThreadManager::getInstance()->terminate();
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    LinuxThreadManager::destroyInstance();
    unregisterLogger(sLogger);
    return LIBDBG_ERR_OK;
}

}  // namespace libdbg

#endif  // LIBDBG_GCC