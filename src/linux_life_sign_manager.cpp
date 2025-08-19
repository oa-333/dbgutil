#include "linux_life_sign_manager.h"

#ifdef DBGUTIL_LINUX

#include <cassert>
#include <cinttypes>
#include <climits>

#include "dbgutil_log_imp.h"
#include "life_sign_manager_internal.h"

namespace dbgutil {

static Logger sLogger;

LinuxLifeSignManager* LinuxLifeSignManager::sInstance = nullptr;

DbgUtilErr LinuxLifeSignManager::createInstance() {
    assert(sInstance == nullptr);
    sInstance = new (std::nothrow) LinuxLifeSignManager();
    assert(sInstance != nullptr);
    return DBGUTIL_ERR_OK;
}

LinuxLifeSignManager* LinuxLifeSignManager::getInstance() {
    assert(sInstance != nullptr);
    return sInstance;
}

void LinuxLifeSignManager::destroyInstance() {
    assert(sInstance != nullptr);
    delete sInstance;
    sInstance = nullptr;
}

DbgUtilErr LinuxLifeSignManager::deleteLifeSignShmSegment(const char* segmentName) {
    return LinuxShm::deleteShm(segmentName);
}

OsShm* LinuxLifeSignManager::createShmObject() {
    m_linuxShm = new (std::nothrow) LinuxShm();
    return m_linuxShm;
}

void LinuxLifeSignManager::destroyShmObject() {
    if (m_linuxShm != nullptr) {
        delete m_linuxShm;
        m_linuxShm = nullptr;
    }
}

DbgUtilErr LinuxLifeSignManager::getImagePath(std::string& imagePath) {
    // can get it only from /proc/self/cmdline
    int fd = open("/proc/self/cmdline", O_RDONLY, 0);
    if (fd == -1) {
        LOG_SYS_ERROR(sLogger, open, "Failed to open /proc/self/cmdline for reading");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // read as many bytes as possible, the program name ends with a null (then program arguments
    // follow, but we don't care about them)
    const size_t BUF_SIZE = 256;
    char buf[BUF_SIZE];
    ssize_t res = read(fd, buf, BUF_SIZE);
    if (res == ((ssize_t)-1)) {
        LOG_SYS_ERROR(sLogger, read, "Failed to read from /proc/self/cmdline for reading");
        close(fd);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    close(fd);

    // search for null and if not found then set one
    bool nullFound = false;
    for (uint32_t i = 0; i < BUF_SIZE; ++i) {
        if (buf[i] == 0) {
            nullFound = true;
            break;
        }
    }
    if (!nullFound) {
        buf[BUF_SIZE - 1] = 0;
    }

    imagePath = buf;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxLifeSignManager::getProcessName(std::string& processName) {
    DbgUtilErr rc = getImagePath(processName);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    const char slash = '/';
    std::string::size_type lastSlashPos = processName.find_last_of(slash);
    if (lastSlashPos != std::string::npos) {
        processName = processName.substr(lastSlashPos + 1);
    }
    return DBGUTIL_ERR_OK;
}

uint32_t LinuxLifeSignManager::getProcessId() { return getpid(); }

std::string LinuxLifeSignManager::getFileTimeStamp() {
    // NOTE: gettimeofday is obsolete, instead clock_gettime() should be used
    // in order to squeeze time to 32 bit we save it from year 2000 offset, this way we get
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t timer = ts.tv_sec;
    struct tm* tm_info = localtime(&timer);

    const size_t BUF_SIZE = 64;
    char buf[BUF_SIZE];
    strftime(buf, BUF_SIZE, "%Y-%m-%d_%H-%M-%S", tm_info);
    std::string fileTimeStamp = buf;
    return fileTimeStamp;
}

const char* LinuxLifeSignManager::getShmPath() { return LinuxShm::getShmPath(); }

DbgUtilErr LinuxLifeSignManager::getShmFileSize(const char* shmFilePath, uint32_t& shmSize) {
    struct stat shmStat = {};
    if (stat(shmFilePath, &shmStat) == -1) {
        LOG_SYS_ERROR(sLogger, stat, "Failed to get shared memory file %s status", shmFilePath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    // must check overflow here
    if (shmStat.st_size >= UINT32_MAX) {
        LOG_ERROR(sLogger,
                  "Internal error: shared memory segment at %s, with size %" PRId64
                  " exceeds expected limit of %u",
                  shmFilePath, shmStat.st_size, UINT32_MAX);
        return DBGUTIL_ERR_INTERNAL_ERROR;
    }
    // cast is ok now
    shmSize = (uint32_t)shmStat.st_size;
    return DBGUTIL_ERR_OK;
}

LinuxLifeSignManager::~LinuxLifeSignManager() {
    if (m_linuxShm != nullptr) {
        m_linuxShm->closeShm();
        delete m_linuxShm;
        m_linuxShm = nullptr;
    }
}

DbgUtilErr initLinuxLifeSignManager() {
    DbgUtilErr rc = initLinuxShm();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    rc = initLifeSignManager();
    if (rc != DBGUTIL_ERR_OK) {
        termLinuxShm();
        return rc;
    }
    registerLogger(sLogger, "linux_life_sign_manager");
    rc = LinuxLifeSignManager::createInstance();
    if (rc != DBGUTIL_ERR_OK) {
        termLinuxShm();
        return rc;
    }
    setLifeSignManager(LinuxLifeSignManager::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLinuxLifeSignManager() {
    setLifeSignManager(nullptr);
    LinuxLifeSignManager::destroyInstance();
    unregisterLogger(sLogger);
    termLifeSignManager();
    termLinuxShm();
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS