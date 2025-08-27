// avoid including windows header, otherwise MinGW compilation fails
#define DBGUTIL_NO_WINDOWS_HEADER
#include "dbg_util_def.h"

// this is good only for Microsoft Visual C++ compiler
// Although this code compiles on MinGW, the g++ compiler does not generate a pdb symbol file
// so most symbol engine functions fail, but this is still useful for stack walking, without symbol
// extraction, which takes place in bfd symbol engine.
#ifdef DBGUTIL_WINDOWS

#ifdef DBGUTIL_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// on MinGW we need this definition for QueueUserAPC2
#ifdef DBGUTIL_MINGW
#define NTDDI_VERSION NTDDI_WIN10_MN
#endif

#include <Windows.h>
#endif

#include <TlHelp32.h>

#include <cassert>
#include <cinttypes>

#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "os_thread_manager_internal.h"
#include "win32_thread_manager.h"

namespace dbgutil {

static Logger sLogger;

Win32ThreadManager* Win32ThreadManager::sInstance = nullptr;

static void apcRoutine(ULONG_PTR data) {
    LOG_DEBUG(sLogger, "Received APC");
    SignalRequest* request = (SignalRequest*)(void*)data;
    request->exec();
}

DbgUtilErr submitThreadSignalRequest(os_thread_id_t osThreadId, SignalRequest* request) {
    HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, osThreadId);
    if (hThread == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, OpenThread, "Failed to get thread %" PRItid " handle", osThreadId);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    if (!QueueUserAPC2(apcRoutine, hThread, (ULONG_PTR)request,
                       QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC)) {
        LOG_WIN32_ERROR(sLogger, QueueUserAPC2, "Failed to queue user APC to thread %" PRItid,
                        osThreadId);
        CloseHandle(hThread);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    CloseHandle(hThread);
    return DBGUTIL_ERR_OK;
}

void Win32ThreadManager::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) Win32ThreadManager();
    assert(sInstance != nullptr);
}

Win32ThreadManager* Win32ThreadManager::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void Win32ThreadManager::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

DbgUtilErr Win32ThreadManager::initialize() { return DBGUTIL_ERR_OK; }

DbgUtilErr Win32ThreadManager::terminate() { return DBGUTIL_ERR_OK; }

// this implementation is available also for MinGW, as it might interact with non-gcc modules
DbgUtilErr Win32ThreadManager::visitThreadIds(ThreadVisitor* visitor) {
    // take a snapshot of all running threads
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, CreateToolhelp32Snapshot, "Failed to get thread snapshot");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // fill in the size of the structure before using it.
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    // retrieve information about the first thread
    if (!Thread32First(hThreadSnap, &te32)) {
        LOG_WIN32_ERROR(sLogger, Thread32First, "Failed to get first thread in snapshot");
        CloseHandle(hThreadSnap);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // Now walk the thread list of the system,
    DWORD currPid = GetCurrentProcessId();
    // and display information about each thread
    // associated with the specified process
    do {
        if (te32.th32OwnerProcessID == currPid) {
            LOG_TRACE(sLogger, "Traversing thread %lu", te32.th32ThreadID);
            visitor->onThreadId(te32.th32ThreadID);
        }
    } while (Thread32Next(hThreadSnap, &te32));

    CloseHandle(hThreadSnap);
    return DBGUTIL_ERR_OK;
}

DbgUtilErr initWin32ThreadManager() {
    registerLogger(sLogger, "win32_thread_manager");
    Win32ThreadManager::createInstance();
    DbgUtilErr rc = Win32ThreadManager::getInstance()->initialize();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
#ifdef DBGUTIL_MSVC
    setThreadManager(Win32ThreadManager::getInstance());
#endif
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termWin32ThreadManager() {
#ifdef DBGUTIL_MSVC
    setThreadManager(nullptr);
#endif
    DbgUtilErr rc = Win32ThreadManager::getInstance()->terminate();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    Win32ThreadManager::destroyInstance();
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS