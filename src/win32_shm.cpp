#include "win32_shm.h"

#ifdef DBGUTIL_WINDOWS

#include "dbgutil_log_imp.h"

namespace dbgutil {

static Logger sLogger;
static std::string sTempPath;

static DbgUtilErr getTempPath(std::string& tempPath);

DbgUtilErr Win32Shm::createShm(const char* name, size_t size, bool shareWrite) {
    LOG_TRACE(sLogger, "Creating SHM %s with size %zu", name, size);
    if (m_shmPtr != nullptr) {
        LOG_ERROR(sLogger, "Cannot create shared memory segment, already open");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // format backing file path
    std::string backingFilePath = sTempPath + name;

    // create backing file
    DWORD shareOpts = FILE_SHARE_READ;
    if (shareWrite) {
        shareOpts |= FILE_SHARE_WRITE;
    }
    // NOTE: we use GENERIC_WRITE so that OS can flush shared memory to disk occasionally
    // TODO: probably more security measures are required here
    m_backingFile = CreateFileA(backingFilePath.c_str(), GENERIC_READ | GENERIC_WRITE, shareOpts,
                                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_backingFile == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, CreateFileA,
                        "Failed to create backing file at %s for shared memory segment by name "
                        "%s with size %zu",
                        backingFilePath.c_str(), name, size);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // create file mapping object on the local session
    // NOTE: backing file size will be increased to match to shared memory segment size
    std::string localName = std::string("Local\\") + name;
    m_mapFile = CreateFileMappingA(m_backingFile,       // use backing file or paging file
                                   nullptr,             // default security
                                   PAGE_READWRITE,      // read/write access
                                   0,                   // maximum object size (high-order DWORD)
                                   (DWORD)size,         // maximum object size (low-order DWORD)
                                   localName.c_str());  // name of mapping object
    if (m_mapFile == nullptr) {
        LOG_WIN32_ERROR(sLogger, CreateFileMappingA,
                        "Failed to create shared memory segment by name %s with size %zu", name,
                        size);
        closeShm();
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // map to process address space
    m_shmPtr = MapViewOfFile(m_mapFile,                       // handle to map object
                             FILE_MAP_READ | FILE_MAP_WRITE,  // read/write permission
                             0, 0, size);
    if (m_shmPtr == nullptr) {
        LOG_WIN32_ERROR(
            sLogger, MapViewOfFile,
            "Failed to map shared memory segment %s to address space of current process", name);
        closeShm();
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    m_name = name;
    m_size = size;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32Shm::openShm(const char* name, size_t size, bool allowWrite /* = false */,
                             bool allowMapBackingFile /* = false */,
                             bool* backingFileMapped /* = nullptr */) {
    if (m_shmPtr != nullptr) {
        LOG_ERROR(sLogger, "Cannot open shared memory segment, already open");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // format backing file path
    std::string backingFilePath = sTempPath + name;

    // NOTE: guardian process requires write access (or anyone else calling syncShm)
    if (allowWrite || allowMapBackingFile) {
        m_backingFile = CreateFileA(backingFilePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_backingFile == INVALID_HANDLE_VALUE) {
            LOG_WIN32_ERROR(sLogger, CreateFileA,
                            "Failed to open backing file at %s of shared memory segment by name "
                            "%s with size %zu",
                            backingFilePath.c_str(), name, size);
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
    }

    // open file mapping by name
    std::string localName = std::string("Local\\") + name;
    // NOTE: write access is required by guardian process
    DWORD mapOpts = FILE_MAP_READ;
    if (allowWrite) {
        mapOpts |= FILE_MAP_WRITE;
    }
    m_mapFile = OpenFileMappingA(mapOpts,             // map permissions
                                 FALSE,               // do not inherit the name
                                 localName.c_str());  // name of mapping object
    if (m_mapFile == nullptr) {
        if (!allowMapBackingFile) {
            LOG_WIN32_ERROR(sLogger, OpenFileMappingA,
                            "Failed to open shared memory segment by name %s", name);
            closeShm();
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        } else {
            // since open shm failed, it means there is no active shm, so in this case there is no
            // writing expected, so we remove write permission in case it was set
            mapOpts = FILE_MAP_READ;
            m_mapFile =
                CreateFileMappingA(m_backingFile, nullptr, PAGE_READONLY, 0, 0, localName.c_str());
            if (m_mapFile == nullptr) {
                LOG_WIN32_ERROR(sLogger, CreateFileMappingA,
                                "Failed to create new shared memory segment mapping to existing "
                                "backing file (name: %s)",
                                name);
                closeShm();
                return DBGUTIL_ERR_SYSTEM_FAILURE;
            }
            if (backingFileMapped != nullptr) {
                *backingFileMapped = true;
            }
        }
    }

    // map to process address space
    // NOTE: write access is required by guardian process
    m_shmPtr = MapViewOfFile(m_mapFile,  // handle to map object
                             mapOpts,    // map permissions
                             0, 0, size);
    if (m_shmPtr == nullptr) {
        LOG_WIN32_ERROR(sLogger, MapViewOfFile,
                        "Failed to map %zu bytes of shared memory segment %s to address space of "
                        "current process",
                        size, name);
        closeShm();
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    m_name = name;
    m_size = size;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32Shm::syncShm() {
    if (m_shmPtr == nullptr) {
        LOG_ERROR(sLogger, "Cannot synchronize shared memory segment %s to disk, not opened",
                  m_name.c_str());
        return DBGUTIL_ERR_INVALID_STATE;
    }
    if (m_backingFile == INVALID_HANDLE_VALUE) {
        LOG_ERROR(
            sLogger,
            "Cannot synchronize shared memory segment %S to disk, shared memory opened without "
            "write access",
            m_name.c_str());
        return DBGUTIL_ERR_INVALID_STATE;
    }

    if (!FlushViewOfFile(m_shmPtr, m_size)) {
        LOG_WIN32_ERROR(sLogger, FlushViewOfFile,
                        "Failed to synchronize shared memory segment %s to disk (%zu bytes)",
                        m_name.c_str(), m_size);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    if (!FlushFileBuffers(m_backingFile)) {
        LOG_WIN32_ERROR(sLogger, FlushFileBuffers,
                        "Failed to flush file buffer for backing file of shared memory segment %s",
                        m_name.c_str());
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32Shm::closeShm() {
    LOG_TRACE(sLogger, "Closing SHM %s with size %zu", m_name.c_str(), m_size);
    if (m_shmPtr != nullptr) {
        if (!UnmapViewOfFile(m_shmPtr)) {
            LOG_WIN32_ERROR(sLogger, UnmapViewOfFile,
                            "Failed to unmap from current process shared memory segment %s, mapped "
                            "at %p, with size %zu",
                            m_name.c_str(), m_shmPtr, m_size);
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
        m_shmPtr = nullptr;
    }

    if (m_mapFile != nullptr) {
        if (!CloseHandle(m_mapFile)) {
            LOG_WIN32_ERROR(sLogger, CloseHandle, "Failed to close shared memory segment %s handle",
                            m_name.c_str());
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
        m_mapFile = nullptr;
    }

    if (m_backingFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_backingFile);
        m_backingFile = INVALID_HANDLE_VALUE;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32Shm::deleteShm(const char* name) {
    // format backing file path
    std::string backingFilePath = sTempPath + name;
    if (!DeleteFileA(backingFilePath.c_str())) {
        LOG_WIN32_ERROR(sLogger, DeleteFile, "Failed to delete shared memory file at path %s",
                        backingFilePath.c_str());
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}

const char* Win32Shm::getShmPath() { return sTempPath.c_str(); }

DbgUtilErr getTempPath(std::string& tempPath) {
    const DWORD BUF_SIZE = MAX_PATH + 2;
    char buf[BUF_SIZE];
    DWORD res = GetTempPathA(BUF_SIZE, buf);
    if (res == 0) {
        LOG_WIN32_ERROR(sLogger, GetTempPathA, "Failed to get temporary files folder");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    if (res < BUF_SIZE) {
        tempPath = buf;
        return DBGUTIL_ERR_OK;
    }

    LOG_WARN(sLogger,
             "Invalid temporary path length %lu, exceeds documented maximum %lu, attempting with a "
             "dynamic buffer",
             res, (DWORD)(MAX_PATH + 1));
    DWORD dynBufLen = res + 1;
    char* dynBuf = new (std::nothrow) char[dynBufLen];
    if (dynBuf == nullptr) {
        LOG_ERROR(sLogger, "Failed to allocate %lu bytes for temporary path buffer, out of memory",
                  dynBufLen);
        return DBGUTIL_ERR_NOMEM;
    }

    DWORD res2 = GetTempPathA(dynBufLen, dynBuf);
    if (res2 == 0) {
        LOG_WIN32_ERROR(sLogger, GetTempPathA,
                        "Failed to get temporary files folder (second time)");
        delete[] dynBuf;
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    if (res2 >= dynBufLen) {
        LOG_ERROR(sLogger,
                  "Invalid temporary path length %lu, exceeds documented maximum %lu and "
                  "dynamic buffer size %;u (second time)",
                  res, (DWORD)(MAX_PATH + 1), dynBufLen);
        delete[] dynBuf;
        return DBGUTIL_ERR_INTERNAL_ERROR;
    }

    tempPath = dynBuf;
    delete[] dynBuf;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr initWin32Shm() {
    registerLogger(sLogger, "win32_shm");
    DbgUtilErr rc = getTempPath(sTempPath);
    if (rc != DBGUTIL_ERR_OK) {
        unregisterLogger(sLogger);
        return rc;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termWin32Shm() {
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

OsShm* createOsShm() { return new (std::nothrow) Win32Shm(); }

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS
