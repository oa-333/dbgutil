// avoid including windows header, otherwise MinGW compilation fails
#define LIBDBG_NO_WINDOWS_HEADER
#include "libdbg_def.h"

// this is good only for Microsoft Visual C++ compiler
// Although this code compiles on MinGW, the g++ compiler does not generate a pdb symbol file
// so most symbol engine functions fail, but this is still useful for stack walking, without symbol
// extraction, which takes place in bfd symbol engine.
#ifdef LIBDBG_WINDOWS

#ifdef LIBDBG_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <tlhelp32.h>

#include <cassert>
#include <cinttypes>

#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "win32_thread_manager.h"

namespace libdbg {

static Logger sLogger;

Win32ThreadManager* Win32ThreadManager::sInstance = nullptr;

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

LibDbgErr Win32ThreadManager::initialize() { return LIBDBG_ERR_OK; }

LibDbgErr Win32ThreadManager::terminate() { return LIBDBG_ERR_OK; }

// this implementation is available also for MinGW, as it might interact with non-gcc modules
LibDbgErr Win32ThreadManager::visitThreadIds(ThreadVisitor* visitor) {
    // take a snapshot of all running threads
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, CreateToolhelp32Snapshot, "Failed to get thread snapshot");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }

    // fill in the size of the structure before using it.
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);

    // retrieve information about the first thread
    if (!Thread32First(hThreadSnap, &te32)) {
        LOG_WIN32_ERROR(sLogger, Thread32First, "Failed to get first thread in snapshot");
        CloseHandle(hThreadSnap);
        return LIBDBG_ERR_SYSTEM_FAILURE;
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
    return LIBDBG_ERR_OK;
}

LibDbgErr initWin32ThreadManager() {
    registerLogger(sLogger, "win32_thread_manager");
    Win32ThreadManager::createInstance();
    LibDbgErr rc = Win32ThreadManager::getInstance()->initialize();
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
#ifdef LIBDBG_MSVC
    setThreadManager(Win32ThreadManager::getInstance());
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr termWin32ThreadManager() {
#ifdef LIBDBG_MSVC
    setThreadManager(nullptr);
#endif
    LibDbgErr rc = Win32ThreadManager::getInstance()->terminate();
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    Win32ThreadManager::destroyInstance();
    unregisterLogger(sLogger);
    return LIBDBG_ERR_OK;
}

}  // namespace libdbg

#endif  // LIBDBG_WINDOWS