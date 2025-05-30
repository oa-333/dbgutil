#include "dbg_util_def.h"

#ifdef DBGUTIL_GCC

#ifdef DBGUTIL_WINDOWS
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
#include <mutex>
#include <unordered_map>

#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "dir_scanner.h"
#include "linux_thread_manager.h"
#include "os_util.h"

#ifdef DBGUTIL_MINGW
#include "win32_thread_manager.h"
#endif

// headers required for dir/file API
#ifndef DBGUTIL_WINDOWS
#include <sys/syscall.h>
#ifdef SYS_tgkill
#define tgkill(tgid, tid, sig) syscall(SYS_tgkill, tgid, tid, sig)
#else
#error "SYS_tgkill unavailable on this platform"
#endif
#endif

#define DBGUTIL_MAX_THREADS 8192
#define DBGUTIL_MAX_THREAD_REQUEST_HANDLERS 16
#define DBGUTIL_INVALID_SLOT_ID ((uint32_t)-1)
#define DBGUTIL_INVALID_THREAD_ID ((os_thread_id_t) - 1)
#define DBGUTIL_INVALID_REQUEST_TYPE ((uint32_t)-1)
#define DBGUTIL_JUMP_START_TIMEOUT_MILLIS 100
#define DBGUTIL_JUMP_START_POLL_FREQ_MILLIS 10

#ifdef DBGUTIL_MINGW
#define SIG_JUMP_START 1
#define SIG_EXEC_REQUEST 2
#else
#define SIG_JUMP_START (SIGRTMIN + 1)
#define SIG_EXEC_REQUEST (SIGRTMIN + 2)
#endif

namespace dbgutil {

static Logger sLogger;

struct ThreadSlot {
public:
    ThreadSlot()
        : m_owner(DBGUTIL_INVALID_THREAD_ID),
          m_isUsed(false),
          m_hasRequest(false),
          m_requestDone(false),
          m_requestorThreadId(0),
          m_requestType(DBGUTIL_INVALID_REQUEST_TYPE),
          m_requestData(nullptr),
          m_requestResult(DBGUTIL_ERR_OK) {}
    ~ThreadSlot() {}

    inline bool tryUse() {
        std::unique_lock<std::mutex> lock(m_lock);
        if (!m_isUsed) {
            m_owner = OsUtil::getCurrentThreadId();
            m_ownerHandle = pthread_self();
            m_isUsed = true;
            m_hasRequest = false;
            m_requestDone = false;
            m_requestorThreadId = 0;
            m_requestType = DBGUTIL_INVALID_REQUEST_TYPE;
            m_requestData = nullptr;
            m_requestResult = DBGUTIL_ERR_OK;
            return true;
        }
        return false;
    }

    inline bool releaseUse() {
        std::unique_lock<std::mutex> lock(m_lock);
        os_thread_id_t threadId = OsUtil::getCurrentThreadId();
        if (m_owner != threadId) {
            LOG_ERROR(sLogger,
                      "Cannot release thread slot, current thread %" PRItid
                      " is not owner thread %" PRItid " of slot",
                      threadId, m_owner);
            return false;
        }
        m_owner = DBGUTIL_INVALID_THREAD_ID;
        m_isUsed = false;
        m_hasRequest = false;
        m_requestDone = false;
        m_requestorThreadId = DBGUTIL_INVALID_THREAD_ID;
        m_requestType = DBGUTIL_INVALID_REQUEST_TYPE;
        m_requestData = nullptr;
        m_requestResult = DBGUTIL_ERR_OK;
        return true;
    }

    inline os_thread_id_t getOwner() const { return m_owner; }

    inline pthread_t getOwnerHandle() const { return m_ownerHandle; }

    inline void postRequest(uint32_t requestType, void* data) {
        // wait until no other thread is holding the slot
        std::unique_lock<std::mutex> lock(m_lock);
        m_cv.wait(lock, [this]() { return !m_hasRequest; });
        m_hasRequest = true;
        m_requestorThreadId = OsUtil::getCurrentThreadId();
        m_requestType = requestType;
        m_requestData = data;
        m_requestDone = false;
    }

    inline void* getRequestData(uint32_t& requestType) {
        std::unique_lock<std::mutex> lock(m_lock);
        if (m_hasRequest) {
            requestType = m_requestType;
            return m_requestData;
        }
        return nullptr;
    }

    inline DbgUtilErr waitRequestDone() {
        // wait until the slot is signaled as ready
        std::unique_lock<std::mutex> lock(m_lock);
        m_cv.wait(lock, [this]() { return m_requestDone; });
        m_requestDone = false;
        return m_requestResult;
    }

    inline void signalRequestDone(DbgUtilErr result) {
        // signal the slot data is ready
        std::unique_lock<std::mutex> lock(m_lock);
        m_requestResult = result;
        m_requestDone = true;
        m_cv.notify_one();
    }

    inline bool releaseRequest() {
        // release hold, notify other threads waiting for hold
        std::unique_lock<std::mutex> lock(m_lock);
        assert(m_hasRequest);
        if (m_requestorThreadId != OsUtil::getCurrentThreadId()) {
            return false;
        }
        m_requestDone = false;
        m_requestorThreadId = DBGUTIL_INVALID_THREAD_ID;
        m_requestData = nullptr;
        return true;
    }

private:
    std::mutex m_lock;
    std::condition_variable m_cv;
    os_thread_id_t m_owner;
    pthread_t m_ownerHandle;
    bool m_isUsed;
    bool m_hasRequest;
    bool m_requestDone;
    os_thread_id_t m_requestorThreadId;
    uint32_t m_requestType;
    void* m_requestData;
    DbgUtilErr m_requestResult;
};

// global thread slot array
static ThreadSlot sThreadSlots[DBGUTIL_MAX_THREADS];

// map from OS thread id to thread slot index
typedef std::unordered_map<os_thread_id_t, uint32_t> ThreadMap;
static ThreadMap sThreadMap;

// thread request handler
struct ThreadRequestHandlerData {
    uint32_t m_requestType;
    LinuxThreadManager::RequestHandler m_handler;
};

// global thread request handler array
static ThreadRequestHandlerData sRequestHandlers[DBGUTIL_MAX_THREAD_REQUEST_HANDLERS];
static uint32_t sRequestHandlerCount = 0;

// global lock for all global data structures
static std::mutex sLock;
static std::atomic<uint64_t> sJumpStartCount = 0;

static uint32_t allocSlot() {
    for (uint32_t i = 0; i < DBGUTIL_MAX_THREADS; ++i) {
        if (sThreadSlots[i].tryUse()) {
            return i;
        }
    }
    return DBGUTIL_INVALID_SLOT_ID;
}

static bool freeSlot(uint32_t slotId) { return sThreadSlots[slotId].releaseUse(); }

static bool getSlotId(os_thread_id_t osThreadId, uint32_t& slotId) {
    // global map access requires lock
    std::unique_lock<std::mutex> lock(sLock);
    ThreadMap::iterator itr = sThreadMap.find(osThreadId);
    if (itr != sThreadMap.end()) {
        slotId = itr->second;
    } else {
        LOG_WARN(sLogger, "Thread handle for OS thread %" PRItid " not found", osThreadId);
        return false;
    }
    if (slotId >= DBGUTIL_MAX_THREADS) {
        LOG_ERROR(sLogger, "Invalid thread slot id out of range: %u", slotId);
        return false;
    }
    return true;
}

static DbgUtilErr addRequestHandler(uint32_t requestType,
                                    LinuxThreadManager::RequestHandler handler) {
    std::unique_lock<std::mutex> lock(sLock);
    if (sRequestHandlerCount == DBGUTIL_MAX_THREAD_REQUEST_HANDLERS) {
        LOG_ERROR(sLogger, "Cannot register request handler with type %u, reached limit",
                  requestType);
        return DBGUTIL_ERR_RESOURCE_LIMIT;
    }
    // make sure there is no duplicate
    for (uint32_t i = 0; i < sRequestHandlerCount; ++i) {
        if (sRequestHandlers[i].m_requestType == requestType) {
            LOG_ERROR(sLogger, "Cannot add thread request handler for request type %u: duplicate",
                      requestType);
            return DBGUTIL_ERR_ALREADY_EXISTS;
        }
    }
    LOG_DEBUG(sLogger, "Registering request handler for request type %u at index %u", requestType,
              sRequestHandlerCount);
    sRequestHandlers[sRequestHandlerCount++] = {requestType, handler};
    return DBGUTIL_ERR_OK;
}

static DbgUtilErr removeRequestHandler(uint32_t requestType) {
    std::unique_lock<std::mutex> lock(sLock);
    bool found = false;
    for (uint32_t i = 0; i < sRequestHandlerCount; ++i) {
        if (sRequestHandlers[i].m_requestType == requestType) {
            sRequestHandlers[i].m_requestType = DBGUTIL_INVALID_REQUEST_TYPE;
            sRequestHandlers[i].m_handler = nullptr;
            found = true;
            LOG_DEBUG(sLogger, "Unregistering request handler for request type %u at index %u",
                      requestType, sRequestHandlerCount);
            break;
        }
    }

    if (found) {
        for (int i = sRequestHandlerCount - 1; i >= 0; --i) {
            if (sRequestHandlers[i].m_handler == nullptr) {
                --sRequestHandlerCount;
            }
        }
        return DBGUTIL_ERR_OK;
    }

    LOG_ERROR(sLogger,
              "Could not unregister request handler for request type %u at index %u: not found",
              requestType, sRequestHandlerCount);
    return DBGUTIL_ERR_NOT_FOUND;
}

static LinuxThreadManager::RequestHandler getRequestHandler(uint32_t requestType) {
    std::unique_lock<std::mutex> lock(sLock);
    for (uint32_t i = 0; i < sRequestHandlerCount; ++i) {
        if (sRequestHandlers[i].m_requestType == requestType) {
            return sRequestHandlers[i].m_handler;
        }
    }
    return nullptr;
}

static void captureThreadStart() {
    // NOTE: no need to lock (caller already locked, be it this thread or another thread)
    os_thread_id_t threadId = OsUtil::getCurrentThreadId();
    LOG_DEBUG(sLogger, "Capturing thread %" PRItid " start", threadId);

    // need to allocate slot for placing stack trace
    uint32_t slotId = allocSlot();
    if (slotId == DBGUTIL_INVALID_SLOT_ID) {
        LOG_WARN(sLogger, "Could not allocate thread slot for thread %" PRItid, threadId);
        threadId = DBGUTIL_INVALID_THREAD_ID;
    } else {
        LOG_DEBUG(sLogger, "Allocated slot %u for thread %" PRItid, slotId, threadId);
        if (!sThreadMap.insert(ThreadMap::value_type(threadId, slotId)).second) {
            LOG_WARN(sLogger, "Could not allocate thread slot for thread %" PRItid, threadId);
            (void)freeSlot(slotId);
            slotId = DBGUTIL_INVALID_SLOT_ID;
            threadId = DBGUTIL_INVALID_THREAD_ID;
        }
    }
    LOG_DEBUG(sLogger, "Jump start count: %" PRIu64,
              sJumpStartCount.load(std::memory_order_relaxed));
    sJumpStartCount.fetch_add(1, std::memory_order_relaxed);
}

static void captureThreadEnd() {
    os_thread_id_t threadId = OsUtil::getCurrentThreadId();
    LOG_DEBUG(sLogger, "Capturing thread %" PRItid " end", threadId);
    std::unique_lock<std::mutex> lock(sLock);

    // find slot id
    uint32_t slotId = DBGUTIL_INVALID_SLOT_ID;
    ThreadMap::iterator itr = sThreadMap.find(threadId);
    if (itr == sThreadMap.end()) {
        LOG_WARN(sLogger, "Thread id %" PRItid " not found in thread map", threadId);
        return;
    }
    slotId = itr->second;
    if (!freeSlot(slotId)) {
        LOG_WARN(sLogger, "Failed to release thread slot %u", slotId);
    }
    sThreadMap.erase(itr);
}

class CaptureThreadId {
public:
    CaptureThreadId() { /*captureThreadStart();*/ }

    ~CaptureThreadId() { captureThreadEnd(); }

    void jumpStart() {
        LOG_DEBUG(sLogger, "Jump starting thread tracking: %" PRItid, OsUtil::getCurrentThreadId());
        captureThreadStart();
    }

private:
    os_thread_id_t m_threadId;
    uint32_t m_slotId;
};

static thread_local CaptureThreadId sCaptureThreadId;

#ifndef DBGUTIL_MINGW
static bool jumpStartThreadTracking(os_thread_id_t osThreadId, uint32_t& slotId) {
    // NOTE: we lock here also on behalf of signaled thread
    std::unique_lock<std::mutex> lock(sLock);

    // for current thread we do it directly without sending a signal
    if (osThreadId == OsUtil::getCurrentThreadId()) {
        sCaptureThreadId.jumpStart();
        return true;
    }

    // jump start
    LOG_DEBUG(sLogger, "Attempting to jump start thread %" PRItid, osThreadId);
    uint64_t targetCount = sJumpStartCount.load(std::memory_order_relaxed) + 1;
    if (tgkill(getpid(), osThreadId, SIG_JUMP_START) == -1) {
        LOG_SYS_ERROR(sLogger, tgkill, "Failed to send jump-start signal to thread %" PRItid,
                      osThreadId);
        return false;
    }

    // wait, but for how long? how can we be sure?
    // TODO: this requires a better strategy
    LOG_DEBUG(sLogger, "Waiting for jump start count %" PRIu64, targetCount);
    uint32_t attempts = DBGUTIL_JUMP_START_TIMEOUT_MILLIS / DBGUTIL_JUMP_START_POLL_FREQ_MILLIS;
    LOG_DEBUG(sLogger, "Waiting for %u millis for %u times, total %u millis",
              DBGUTIL_JUMP_START_POLL_FREQ_MILLIS, attempts, DBGUTIL_JUMP_START_TIMEOUT_MILLIS);
    for (uint32_t i = 0; i < attempts + 1000; ++i) {
        if (sJumpStartCount.load(std::memory_order_relaxed) < targetCount) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(DBGUTIL_JUMP_START_POLL_FREQ_MILLIS));
        } else {
            break;
        }
    }
    LOG_DEBUG(sLogger, "Waiting DONE");

    // check if timed out
    if (sJumpStartCount.load(std::memory_order_relaxed) < targetCount) {
        LOG_TRACE(sLogger, "Jump start for thread %" PRItid " timed out", osThreadId);
        return false;
    }
    LOG_DEBUG(sLogger, "Jump start SUCCESS");

    // try again
    LOG_DEBUG(sLogger, "Jump start for thread %" PRItid " succeeded, trying again", osThreadId);
    ThreadMap::iterator itr = sThreadMap.find(osThreadId);
    if (itr == sThreadMap.end()) {
        LOG_ERROR(sLogger, "Thread slot for for thread %" PRItid " not found after jump start",
                  osThreadId);
        return false;
    }
    slotId = itr->second;
    if (slotId >= DBGUTIL_MAX_THREADS) {
        LOG_ERROR(sLogger, "Invalid thread %" PRItid " slot after jump start, out of range: %u",
                  osThreadId, slotId);
        return false;
    }
    LOG_DEBUG(sLogger, "Thread %" PRItid " collected slot %u after jump start", osThreadId, slotId);
    return true;
}
#endif

/*bool getPThreadId(os_thread_id_t threadId, pthread_t& pthreadId) {
    ThreadMap::iterator itr = sThreadMap.find(threadId);
    if (itr != sThreadMap.end()) {
        pthreadId = sThreadSlots[itr->second].getOwner();
        return true;
    }
    return false;
}*/

LinuxThreadManager* LinuxThreadManager::sInstance = nullptr;

static void execCurrentThreadRequest() {
    os_thread_id_t osThreadId = OsUtil::getCurrentThreadId();
    LOG_DEBUG(sLogger, "Executing thread %" PRItid " request", osThreadId);
    uint32_t slotId = DBGUTIL_INVALID_SLOT_ID;
    if (getSlotId(osThreadId, slotId)) {
        // no lock needed from here on
        uint32_t requestType = DBGUTIL_INVALID_REQUEST_TYPE;
        void* data = sThreadSlots[slotId].getRequestData(requestType);
        LinuxThreadManager::RequestHandler handler = getRequestHandler(requestType);
        if (handler == nullptr) {
            LOG_ERROR(sLogger, "Cannot execute thread request: invalid thread request type %u",
                      requestType);
            sThreadSlots[slotId].signalRequestDone(DBGUTIL_ERR_INVALID_ARGUMENT);
        } else {
            // execute handler
            DbgUtilErr rc = handler(data);
            LOG_DEBUG(sLogger, "Request execution ended with result: %s", errorCodeToStr(rc));
            sThreadSlots[slotId].signalRequestDone(rc);
        }
    }
}

static void signalHandler(int sigNum) {
    if (sigNum == SIG_JUMP_START) {
        LOG_DEBUG(sLogger, "Signal handler: Jump starting thread tracking: %" PRItid,
                  OsUtil::getCurrentThreadId());
        sCaptureThreadId.jumpStart();
        // captureThreadStart();
    } else if (sigNum == SIG_EXEC_REQUEST) {
        LOG_DEBUG(sLogger, "Signal handler: Executing thread request: %" PRItid,
                  OsUtil::getCurrentThreadId());
        execCurrentThreadRequest();
    } else {
        LOG_WARN(sLogger, "Unexpected signal number %d (ignored)", sigNum);
    }
}

#ifdef DBGUTIL_MINGW
static void apcRoutine(ULONG_PTR sigNum) { signalHandler((int)sigNum); }
#endif

#ifndef DBGUTIL_MINGW
static DbgUtilErr registerSignalHandler(int sigNum) {
    if (signal(sigNum, signalHandler) == SIG_ERR) {
        LOG_SYS_ERROR(sLogger, signal, "Failed to register signal %d handler", sigNum);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    LOG_DEBUG(sLogger, "Registered signal %d handler", sigNum);
    return DBGUTIL_ERR_OK;
}

static DbgUtilErr unregisterSignalHandler(int sigNum) {
    if (signal(sigNum, nullptr) == SIG_ERR) {
        LOG_SYS_ERROR(sLogger, signal, "Failed to unregister signal %d handler", sigNum);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    LOG_DEBUG(sLogger, "Unregistered signal %d handler", sigNum);
    return DBGUTIL_ERR_OK;
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

DbgUtilErr LinuxThreadManager::initialize() {
#ifdef DBGUTIL_LINUX
    DbgUtilErr rc = registerSignalHandler(SIG_JUMP_START);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    return registerSignalHandler(SIG_EXEC_REQUEST);
#else
    // MinGW does not implement signals well, so we need a workaround using QueueUserAPC2
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr LinuxThreadManager::terminate() {
#ifdef DBGUTIL_LINUX
    DbgUtilErr rc = unregisterSignalHandler(SIG_EXEC_REQUEST);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    return unregisterSignalHandler(SIG_JUMP_START);
#else
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr LinuxThreadManager::visitThreads(ThreadListener* listener) {
#ifdef DBGUTIL_MINGW
    // divert to Win32
    return Win32ThreadManager::getInstance()->visitThreads(listener);
#else
    // take a snapshot of all running threads through /proc/self/task
    std::vector<std::string> dirNames;
    DbgUtilErr rc = DirScanner::scanDirDirs("/proc/self/task", dirNames);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to list directory entries under /proc/self/task: %s",
                  errorCodeToStr(rc));
        return rc;
    }

    for (const std::string& taskIdName : dirNames) {
        os_thread_id_t osThreadId = 0;
        std::size_t pos = 0;
        try {
            osThreadId = std::stol(taskIdName, &pos);
        } catch (std::exception& e) {
            LOG_SYS_ERROR(sLogger, std::stoull,
                          "Failed to convert Linux task name %s to integer value: %s",
                          taskIdName.c_str(), e.what());
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
        if (pos != taskIdName.length()) {
            LOG_ERROR(sLogger,
                      "Excess characters at Linux task id name, integer conversion failed: %s",
                      taskIdName.c_str());
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }

        // now we need to search for the corresponding pthread_t object
        uint32_t slotId = DBGUTIL_INVALID_SLOT_ID;
        if (getSlotId(osThreadId, slotId)) {
            ThreadId threadId = {osThreadId, (void*)(uint64_t)slotId};
            listener->onThread(threadId);
        } else if (jumpStartThreadTracking(osThreadId, slotId)) {
            ThreadId threadId = {osThreadId, (void*)(uint64_t)slotId};
            listener->onThread(threadId);
        }
    }

    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr LinuxThreadManager::getThreadHandle(const ThreadId& threadId, pthread_t& threadHandle) {
    uint32_t slotId = (uint32_t)(uint64_t)threadId.m_threadData;
    if (slotId >= DBGUTIL_MAX_THREADS) {
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }
    // NOTE: we don't really need a lock, this is a static array
    threadHandle = sThreadSlots[slotId].getOwnerHandle();
    if (threadHandle == DBGUTIL_INVALID_THREAD_ID) {
        return DBGUTIL_ERR_NOT_FOUND;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxThreadManager::registerThreadRequestHandler(uint32_t requestType,
                                                            RequestHandler handler) {
    return addRequestHandler(requestType, handler);
}

DbgUtilErr LinuxThreadManager::unregisterThreadRequestHandler(uint32_t requestType) {
    return removeRequestHandler(requestType);
}

DbgUtilErr LinuxThreadManager::execThreadRequest(const ThreadId& threadId, uint32_t requestType,
                                                 void* requestData, DbgUtilErr& requestResult) {
    uint32_t slotId = (uint32_t)(uint64_t)threadId.m_threadData;
    if (slotId >= DBGUTIL_MAX_THREADS) {
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }
    // NOTE: we don't really need a lock, this is a static array
    ThreadSlot& threadSlot = sThreadSlots[slotId];
    threadSlot.postRequest(requestType, requestData);
#ifdef DBGUTIL_LINUX
    int res = pthread_kill(threadSlot.getOwnerHandle(), SIG_EXEC_REQUEST);
    if (res != 0) {
        LOG_SYS_ERROR_NUM(sLogger, pthread_kill, res, "Failed to send signal to thread %" PRItid,
                          threadId.m_osThreadId);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
#else
    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, threadId.m_osThreadId);
    if (hThread == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, OpenThread, "Failed to get thread %" PRItid " handle",
                        threadId.m_osThreadId);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // if (!QueueUserAPC2(apcRoutine, hThread, NULL, QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC)) {
    if (!QueueUserAPC(apcRoutine, hThread, (ULONG_PTR)SIG_EXEC_REQUEST)) {
        LOG_WIN32_ERROR(sLogger, QueueUserAPC, "Failed to queue user APC to thread %" PRItid,
                        threadId.m_osThreadId);
        CloseHandle(hThread);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    CloseHandle(hThread);
#endif
    requestResult = threadSlot.waitRequestDone();
    if (!threadSlot.releaseRequest()) {
        LOG_WARN(sLogger, "Invalid thread request state after request execution (ignored)");
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr initLinuxThreadManager() {
    registerLogger(sLogger, "linux_thread_manager");
    LinuxThreadManager::createInstance();
    DbgUtilErr rc = LinuxThreadManager::getInstance()->initialize();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    setThreadManager(LinuxThreadManager::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLinuxThreadManager() {
    setThreadManager(nullptr);
    DbgUtilErr rc = LinuxThreadManager::getInstance()->terminate();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    LinuxThreadManager::destroyInstance();
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

#if 0
BEGIN_STARTUP_JOB(LinuxThreadManager) {
    sLogger = elog::ELogSystem::getSharedLogger("csi.common.linux_thread_manager");
    LinuxThreadManager::createInstance();
    DbgUtilErr rc = LinuxThreadManager::getInstance()->initialize();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
#ifdef DBGUTIL_LINUX
    setThreadManager(LinuxThreadManager::getInstance());
#endif
    return DBGUTIL_ERR_OK;
}
END_STARTUP_JOB(LinuxThreadManager)

BEGIN_TEARDOWN_JOB(LinuxThreadManager) {
    setThreadManager(nullptr);
    DbgUtilErr rc = LinuxThreadManager::getInstance()->terminate();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
#ifdef DBGUTIL_LINUX
    LinuxThreadManager::destroyInstance();
#endif
    return DBGUTIL_ERR_OK;
}
END_TEARDOWN_JOB(LinuxThreadManager)
#endif

}  // namespace dbgutil

#endif  // DBGUTIL_GCC