#include "linux_shm.h"

#ifdef DBGUTIL_LINUX

#include "dbgutil_log_imp.h"

namespace dbgutil {

static Logger sLogger;

DbgUtilErr LinuxShm::createShm(const char* name, size_t size, bool shareWrite) {
    if (m_shmPtr != nullptr) {
        LOG_ERROR(sLogger, "Cannot create shared memory segment, already open");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    m_name = name;
    m_size = size;

    // open shared memory segment
    int shareOpts = S_IRUSR;
    if (shareWrite) {
        shareOpts |= S_IWUSR;
    }
    m_shmFd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, shareOpts);
    if (m_shmFd == -1) {
        LOG_SYS_ERROR(sLogger, shm_open, "Failed to create shared memory segment by name %s", name);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // set the size of the shared memory object
    if (ftruncate(m_shmFd, size) == -1) {
        LOG_SYS_ERROR(sLogger, ftruncate, "Failed to set shared memory segment %s size to %zu",
                      name, size);
        closeShm();
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // Map the shared memory object into the process's address space
    m_shmPtr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0);
    if (m_shmPtr == MAP_FAILED) {
        LOG_SYS_ERROR(sLogger, mmap,
                      "Failed to map shared memory segment %s to address space of current process",
                      name);
        closeShm();
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxShm::openShm(const char* name, size_t size, bool allowWrite,
                             bool allowMapBackingFile) {
    (void)allowMapBackingFile;
    if (m_shmPtr != nullptr) {
        LOG_ERROR(sLogger, "Cannot open shared memory segment, already open");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    m_name = name;
    m_size = size;

    // open shared memory segment
    int openMode = O_RDONLY;
    if (allowWrite) {
        openMode = O_RDWR;
    }
    m_shmFd = shm_open(name, openMode, 0);
    if (m_shmFd == -1) {
        LOG_SYS_ERROR(sLogger, shm_open, "Failed to open shared memory segment by name %s", name);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // Map the shared memory object into the process's address space
    int protOpts = PROT_READ;
    if (allowWrite) {
        protOpts |= PROT_WRITE;
    }
    m_shmPtr = mmap(0, size, protOpts, MAP_SHARED, m_shmFd, 0);
    if (m_shmPtr == MAP_FAILED) {
        LOG_SYS_ERROR(sLogger, mmap,
                      "Failed to map %zu bytes of shared memory segment %s to address space of "
                      "current process",
                      size, name);
        closeShm();
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxShm::closeShm() {
    if (m_shmPtr != nullptr) {
        if (munmap(m_shmPtr, m_size) == -1) {
            LOG_SYS_ERROR(sLogger, mmap,
                          "Failed to unmap from current process shared memory segment %s, mapped "
                          "at %p, with size %zu",
                          m_name.c_str(), m_shmPtr, m_size);
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
        m_shmPtr = nullptr;
    }

    if (m_shmFd != DBGUTIL_LINUX_SHM_INVALID_FD) {
        if (close(m_shmFd) == -1) {
            LOG_SYS_ERROR(sLogger, close,
                          "Failed to close shared memory segment %s file descriptor",
                          m_name.c_str());
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
        m_shmFd = DBGUTIL_LINUX_SHM_INVALID_FD;
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr LinuxShm::deleteShm(const char* segmentName) {
    if (shm_unlink(segmentName) == -1) {
        LOG_SYS_ERROR(sLogger, mmap, "Failed to unlink shared memory segment %s", segmentName);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}

const char* LinuxShm::getShmPath() { return "/dev/shm/"; }

DbgUtilErr initLinuxShm() {
    registerLogger(sLogger, "linux_shm");
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLinuxShm() {
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

OsShm* createOsShm() { return new (std::nothrow) LinuxShm(); }

}  // namespace dbgutil

#endif