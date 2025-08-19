#include "win32_life_sign_manager.h"

#ifdef DBGUTIL_WINDOWS

#include <cassert>

#include "dbgutil_log_imp.h"
#include "life_sign_manager_internal.h"

namespace dbgutil {

static Logger sLogger;

Win32LifeSignManager* Win32LifeSignManager::sInstance = nullptr;

DbgUtilErr Win32LifeSignManager::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) Win32LifeSignManager();
    assert(sInstance != nullptr);
    return DBGUTIL_ERR_OK;
}

Win32LifeSignManager* Win32LifeSignManager::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void Win32LifeSignManager::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

DbgUtilErr Win32LifeSignManager::deleteLifeSignShmSegment(const char* segmentName) {
    return Win32Shm::deleteShm(segmentName);
}

OsShm* Win32LifeSignManager::createShmObject() {
    m_win32Shm = new (std::nothrow) Win32Shm();
    return m_win32Shm;
}

void Win32LifeSignManager::destroyShmObject() {
    if (m_win32Shm != nullptr) {
        delete m_win32Shm;
        m_win32Shm = nullptr;
    }
}

DbgUtilErr Win32LifeSignManager::getImagePath(std::string& imagePath) {
    // we use documented _pgmptr to get the full path of the executable file
    char* progPtr = nullptr;
    errno_t rc = _get_pgmptr(&progPtr);
    if (rc != 0) {
        LOG_SYS_ERROR_NUM(sLogger, _get_pgmptr, rc, "Failed to get program name");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    imagePath = progPtr;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32LifeSignManager::getProcessName(std::string& processName) {
    DbgUtilErr rc = getImagePath(processName);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    const char slash = '\\';
    std::string::size_type lastSlashPos = processName.find_last_of(slash);
    if (lastSlashPos != std::string::npos) {
        processName = processName.substr(lastSlashPos + 1);
    }
    return DBGUTIL_ERR_OK;
}

uint32_t Win32LifeSignManager::getProcessId() { return ::GetCurrentProcessId(); }

std::string Win32LifeSignManager::getFileTimeStamp() {
    SYSTEMTIME stTime = {};
    GetLocalTime(&stTime);
    const size_t BUF_SIZE = 64;
    char buf[BUF_SIZE];
    snprintf(buf, BUF_SIZE, "%hu-%.2hu-%.2hu_%.2hu-%.2hu-%.2hu", stTime.wYear, stTime.wMonth,
             stTime.wDay, stTime.wHour, stTime.wMinute, stTime.wSecond);
    std::string fileTimeStamp = buf;
    return fileTimeStamp;
}

const char* Win32LifeSignManager::getShmPath() { return Win32Shm::getShmPath(); }

DbgUtilErr Win32LifeSignManager::getShmFileSize(const char* shmFilePath, uint32_t& shmSize) {
    WIN32_FILE_ATTRIBUTE_DATA attrs = {};
    if (!GetFileAttributesExA(shmFilePath, GetFileExInfoStandard, &attrs)) {
        LOG_WIN32_ERROR(sLogger, GetFileAttributesExA,
                        "Failed to get attributes of shared memory backing file at %s",
                        shmFilePath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    ULARGE_INTEGER fileSize = {};
    fileSize.HighPart = attrs.nFileSizeHigh;
    fileSize.LowPart = attrs.nFileSizeLow;
    if (attrs.nFileSizeHigh != 0) {
        LOG_ERROR(sLogger, "Unexpected file size (too large): %" PRId64, fileSize.QuadPart);
        return DBGUTIL_ERR_INTERNAL_ERROR;
    }
    shmSize = attrs.nFileSizeLow;
    return DBGUTIL_ERR_OK;
}

Win32LifeSignManager::~Win32LifeSignManager() {
    if (m_win32Shm != nullptr) {
        m_win32Shm->closeShm();
        delete m_win32Shm;
        m_win32Shm = nullptr;
    }
}

DbgUtilErr initWin32LifeSignManager() {
    DbgUtilErr rc = initWin32Shm();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    rc = initLifeSignManager();
    if (rc != DBGUTIL_ERR_OK) {
        termWin32Shm();
        return rc;
    }
    registerLogger(sLogger, "win32_life_sign_manager");
    rc = Win32LifeSignManager::createInstance();
    if (rc != DBGUTIL_ERR_OK) {
        termWin32Shm();
        return rc;
    }
    setLifeSignManager(Win32LifeSignManager::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termWin32LifeSignManager() {
    setLifeSignManager(nullptr);
    Win32LifeSignManager::destroyInstance();
    unregisterLogger(sLogger);
    termLifeSignManager();
    termWin32Shm();
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS