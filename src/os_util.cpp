#include "dbg_util_def.h"

#ifdef DBGUTIL_MINGW
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cstring>

#include "dbg_stack_trace.h"
#include "dbgutil_log_imp.h"
#include "os_util.h"

// headers required for dir/file API
#ifndef DBGUTIL_WINDOWS
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef SYS_gettid
#define gettid() syscall(SYS_gettid)
#else
#error "SYS_gettid unavailable on this system"
#endif
#endif

// headers required for fsync/fdatasync
#ifdef DBGUTIL_WINDOWS
#include <io.h>
#include <winternl.h>
#endif

#define SIM_FDATASYNC
#ifndef SIM_FDATASYNC
// NtFlushBuffersFileEx
#include "ntifs.h"
#define FLUSH_FLAGS_FILE_DATA_SYNC_ONLY 3
extern NTSTATUS NtFlushBuffersFileEx(HANDLE FileHandle, ULONG Flags, PVOID Parameters,
                                     ULONG ParametersSize, PIO_STATUS_BLOCK IoStatusBlock);
extern int initialize_ntdll();

// RtlNtStatusToDosError
#include <system_error>

#include "winternl.h"
#endif

namespace dbgutil {

static Logger sLogger;

void OsUtil::initLogger() { registerLogger(sLogger, "os_util"); }

os_thread_id_t OsUtil::getCurrentThreadId() {
#ifdef DBGUTIL_WINDOWS
    return GetCurrentThreadId();
#else
    return gettid();
#endif
}

DbgUtilErr OsUtil::deleteFile(const char* filePath) {
#ifdef DBGUTIL_WINDOWS
    if (!DeleteFileA(filePath)) {
        LOG_WIN32_ERROR(sLogger, DeleteFile, "Failed to delete file %s", filePath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
#else
    if (unlink(filePath) == -1) {
        LOG_SYS_ERROR(sLogger, unlink, "Failed to delete file %s", filePath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr OsUtil::fileExists(const char* filePath) {
#ifdef DBGUTIL_WINDOWS
    DWORD dwAttrib = GetFileAttributesA(filePath);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        DWORD rc = GetLastError();
        if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) {
            return DBGUTIL_ERR_NOT_FOUND;
        }
        LOG_WIN32_ERROR_NUM(sLogger, GetFileAttributes, rc, "Failed to check file %s existence",
                            filePath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    if (!(dwAttrib & FILE_ATTRIBUTE_NORMAL) && !(dwAttrib & FILE_ATTRIBUTE_ARCHIVE)) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    return DBGUTIL_ERR_OK;
#else
    struct stat buf = {};
    if (lstat(filePath, &buf) != 0) {
        int rc = errno;
        if (rc == ENOENT) {
            return DBGUTIL_ERR_NOT_FOUND;
        }
        LOG_SYS_ERROR_NUM(sLogger, lstat, rc, "Failed to check file %s existence", filePath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    if (!S_ISREG(buf.st_mode)) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr OsUtil::dirExists(const char* dirPath) {
#ifdef DBGUTIL_WINDOWS
    DWORD dwAttrib = GetFileAttributesA(dirPath);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        DWORD rc = GetLastError();
        if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) {
            return DBGUTIL_ERR_NOT_FOUND;
        }
        LOG_WIN32_ERROR_NUM(sLogger, GetFileAttributes, rc, "Failed to check file %s existence",
                            dirPath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    if (!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    return DBGUTIL_ERR_OK;
#else
    struct stat buf = {};
    if (lstat(dirPath, &buf) != 0) {
        int rc = errno;
        if (rc == ENOENT) {
            return DBGUTIL_ERR_NOT_FOUND;
        }
        LOG_SYS_ERROR_NUM(sLogger, lstat, rc, "Failed to check file %s existence", dirPath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    if (!S_ISDIR(buf.st_mode)) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr OsUtil::getCurrentDir(std::string& currentDir) {
#ifdef DBGUTIL_WINDOWS
    DWORD len = GetCurrentDirectoryA(0, nullptr);
    if (len == 0) {
        LOG_WIN32_ERROR(sLogger, GetCurrentDirectoryA,
                        "Failed to retrieve current working directory");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    char* buf = (char*)malloc(len + 1);
    if (buf == nullptr) {
        LOG_SYS_ERROR(sLogger, malloc,
                      "Failed to allocate %u bytes for retrieving current directory", len);
        return DBGUTIL_ERR_NOMEM;
    }
    if (GetCurrentDirectoryA(len + 1, buf) == 0) {
        LOG_WIN32_ERROR(sLogger, GetCurrentDirectoryA,
                        "Failed to retrieve current working directory");
        free(buf);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    currentDir = buf;
    free(buf);
    buf = nullptr;
    return DBGUTIL_ERR_OK;
#else
    char buf[PATH_MAX];
    if (getcwd(buf, PATH_MAX) == nullptr) {
        LOG_SYS_ERROR(sLogger, getcwd, "Failed to retrieve current working directory");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    currentDir = buf;
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr OsUtil::createDir(const char* path) {
#ifdef DBGUTIL_WINDOWS
    if (!CreateDirectoryA(path, nullptr)) {
        DWORD rc = GetLastError();
        if (rc == ERROR_ALREADY_EXISTS) {
            return DBGUTIL_ERR_ALREADY_EXISTS;
        }
        LOG_WIN32_ERROR(sLogger, CreateDirectoryA, "Failed to create directory: %s", path);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
#else
    if (mkdir(path, 0700) != 0) {
        int rc = errno;
        if (rc == EEXIST) {
            return DBGUTIL_ERR_ALREADY_EXISTS;
        }
        LOG_SYS_ERROR(sLogger, mkdir, "Failed to create directory: %s", path);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
#endif
}

// TODO: add utility to map system error code to internal error code or default to E_SYSTEM_FAILURE.
DbgUtilErr OsUtil::deleteDir(const char* path) {
#ifdef DBGUTIL_WINDOWS
    if (!RemoveDirectoryA(path)) {
        DWORD rc = GetLastError();
        if (rc == ERROR_NOT_FOUND) {
            return DBGUTIL_ERR_NOT_FOUND;
        }
        LOG_WIN32_ERROR(sLogger, CreateDirectoryA, "Failed to delete directory: %s", path);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
#else
    if (rmdir(path) != 0) {
        int rc = errno;
        if (rc == ENOENT) {
            return DBGUTIL_ERR_NOT_FOUND;
        }
        LOG_SYS_ERROR(sLogger, mkdir, "Failed to delete directory: %s", path);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr OsUtil::openFile(const char* path, int flags, int mode, int& fd) {
#ifdef DBGUTIL_WINDOWS
    fd = _open(path, flags, mode);
#else
    fd = open(path, flags, mode);
#endif
    if (fd == -1) {
        LOG_SYS_ERROR(sLogger, open, "Failed to open file: %s", path);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::closeFile(int fd) {
#ifdef DBGUTIL_WINDOWS
    int res = _close(fd);
#else
    int res = close(fd);
#endif
    if (res == -1) {
        LOG_SYS_ERROR(sLogger, close, "Failed to close file descriptor");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::seekFile(int fd, uint64_t offset, int origin,
                            uint64_t* resultOffset /* = nullptr */, int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    long long res = _lseeki64(fd, offset, origin);
#else
    off64_t res = lseek64(fd, offset, origin);
#endif
    if (res == ((off_t)-1)) {
        if (sysErr) {
            *sysErr = errno;
        }
        LOG_SYS_ERROR(sLogger, lseek, "Failed to seek file");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    if (resultOffset != nullptr) {
        *resultOffset = (uint64_t)res;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::getFileOffset(int fd, uint64_t& offset, int* sysErr /* = nullptr */) {
    return seekFile(fd, 0, SEEK_CUR, &offset, sysErr);
}

DbgUtilErr OsUtil::getFileSize(int fd, uint64_t& fileSizeBytes, int* sysErr /* = nullptr */) {
    // first save current offset
    uint64_t currOffset = 0;
    DbgUtilErr rc = getFileOffset(fd, currOffset, sysErr);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // now seek to end
    rc = seekFile(fd, 0, SEEK_END, &fileSizeBytes, sysErr);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // restore previous offset
    return seekFile(fd, currOffset, SEEK_SET, nullptr, sysErr);
}

DbgUtilErr OsUtil::writeFile(int fd, const char* buf, uint32_t len, uint32_t& bytesWritten,
                             int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    int res = _write(fd, buf, len);
#else
    ssize_t res = write(fd, buf, len);
#endif
    if (res == ((ssize_t)-1)) {
        if (sysErr != nullptr) {
            *sysErr = errno;
        }
        // refrain from writing to log, this may cause log flooding in case of intense writing
        // LOG_SYS_ERROR(sLogger, write, "Failed to write to file");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    bytesWritten = res;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::readFile(int fd, char* buf, uint32_t len, uint32_t& bytesRead,
                            int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    int res = _read(fd, buf, len);
#else
    ssize_t res = read(fd, buf, len);
#endif
    if (res == ((ssize_t)-1)) {
        if (sysErr != nullptr) {
            *sysErr = errno;
        }
        // refrain from writing to log, this may cause log flooding in case of intense reading
        // LOG_SYS_ERROR(sLogger, write, "Failed to read from file");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    bytesRead = res;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::fsyncFile(int fd, int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    DbgUtilErr rc = DBGUTIL_ERR_OK;
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        rc = DBGUTIL_ERR_INVALID_ARGUMENT;
    } else {
        if (!FlushFileBuffers(handle)) {
            // try to translate some Windows errors into rough approximations of POSIX errors
            DWORD err = GetLastError();
            switch (err) {
                case ERROR_ACCESS_DENIED:
                    // For a read-only handle, fsync should succeed, even though we have no way to
                    // sync the access-time changes
                    errno = EACCES;
                    rc = DBGUTIL_ERR_ACCESS_DENIED;
                    break;

                case ERROR_INVALID_HANDLE:
                    // probably trying to fsync a tty
                    errno = EINVAL;
                    rc = DBGUTIL_ERR_INVALID_ARGUMENT;
                    break;

                default:
                    errno = EIO;
                    rc = DBGUTIL_ERR_SYSTEM_FAILURE;
                    break;
            }
        }
    }

    if (rc != DBGUTIL_ERR_OK && sysErr != nullptr) {
        *sysErr = errno;
    }
    return rc;
#else
    if (fsync(fd) == -1) {
        if (sysErr != nullptr) {
            *sysErr = errno;
        }
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr OsUtil::fdatasyncFile(int fd, int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    // TODO: implement this
#ifdef SIM_FDATASYNC
    return fsyncFile(fd, sysErr);
#else
    DbgUtilErr rc = DBGUTIL_ERR_OK;
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        rc = DBGUTIL_ERR_INVALID_ARGUMENT;
    } else {
        if (initialize_ntdll() < 0) {
            errno = EFAULT;  //  TODO: is this meaningful?
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }

        IO_STATUS_BLOCK ioStatBlock = {};
        memset(&ioStatBlock, 0, sizeof(&ioStatBlock));
        NTSTATUS status =
            NtFlushBuffersFileEx(handle, FLUSH_FLAGS_FILE_DATA_SYNC_ONLY, NULL, 0, &ioStatBlock);
        if (NT_SUCCESS(status)) {
            return DBGUTIL_ERR_OK;
        }

        // TODO: this should be fixed
        ULONG errCode = RtlNtStatusToDosError(status);
        SetLastError(errCode);
        rc = DBGUTIL_ERR_SYSTEM_FAILURE;
        errno = EFAULT;  //  TODO: is this meaningful?
        /*const std::error_condition errCond =
            std::system_category().default_error_condition(errCode);
        return errCond.value();*/
    }

    if (rc != DBGUTIL_ERR_OK && sysErr != nullptr) {
        *sysErr = errno;
    }
    return rc;
#endif
#else
    if (fsync(fd) == -1) {
        if (sysErr != nullptr) {
            *sysErr = errno;
        }
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
#endif
}

DbgUtilErr OsUtil::readEntireFileToBuf(const char* path, std::vector<char>& buf) {
    int fd = 0;
    DbgUtilErr rc = openFile(path, O_BINARY | O_RDONLY, 0, fd);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // some files cannot be seek-ed, such as /proc files, so we just read in chunks of 4K
    const int BUF_INC_SIZE = 4096;
    int offset = 0;
    uint32_t bytesRead = 0;
    do {
        if (buf.size() - offset == 0) {
            buf.resize(buf.size() + BUF_INC_SIZE);
        }
        rc = readFile(fd, &buf[offset], buf.size() - offset, bytesRead);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        offset += bytesRead;
    } while (bytesRead == 0);
    // NOTE: reading less than requested does not necessarily indicate end of file, according to man
    // page only return value of 0 truly indicates end of file

    return closeFile(fd);
}

DbgUtilErr OsUtil::readEntireFileToLines(const char* path, std::vector<std::string>& lines) {
    std::vector<char> buf;
    DbgUtilErr rc = readEntireFileToBuf(path, buf);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // break to lines
    int prevIdx = -1;
    do {
        int idx = 0;
        std::vector<char>::iterator itr = std::find(buf.begin() + prevIdx + 1, buf.end(), '\n');
        if (itr == buf.end()) {
            idx = buf.size();
        } else {
            idx = itr - buf.begin();
        }

        std::string line(&buf[prevIdx + 1], idx - prevIdx - 1);
        lines.push_back(line);
        prevIdx = idx;
    } while (prevIdx < buf.size());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::execCmd(const char* cmdLine, std::vector<char>& buf) {
#ifdef DBGUTIL_WINDOWS
    FILE* output = _popen(cmdLine, "r");
#else
    FILE* output = popen(cmdLine, "r");
#endif
    if (output == nullptr) {
        LOG_SYS_ERROR(sLogger, popen, "Failed to execute command: %s", cmdLine);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    buf.resize(4096);
    uint32_t pos = 0;
    uint32_t bufSize = buf.size() - pos;
    // fgets reads at most bufSize-1 puts a terminating null
    // newline chars are copied as is
    while (fgets(&buf[pos], bufSize, output)) {
        pos += strlen(&buf[pos]);
        if (buf.size() - pos < 1024) {
            buf.resize(buf.size() * 2);
        }
        bufSize = buf.size() - pos;
    }

    int isEof = feof(output);
#ifdef DBGUTIL_WINDOWS
    int res = _pclose(output);
#else
    int res = pclose(output);
#endif
    if (res != 0) {
        LOG_ERROR(sLogger, "Cmd line execution finished with error %d", res);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    if (!isEof) {
        LOG_ERROR(sLogger, "Failed to read command line output till the end");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::initializeSpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    InitializeCriticalSection(&spinLock);
#else
    int res = pthread_spin_init(&spinLock, 0);
    if (res != 0) {
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
#endif
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::destroySpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    DeleteCriticalSection(&spinLock);
#else
    int res = pthread_spin_destroy(&spinLock);
    if (res != 0) {
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
#endif
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::lockSpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    EnterCriticalSection(&spinLock);
#else
    int res = pthread_spin_lock(&spinLock);
    if (res != 0) {
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
#endif
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::tryLockSpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    if (!TryEnterCriticalSection(&spinLock)) {
        return DBGUTIL_ERR_RESOURCE_BUSY;
    }
#else
    int res = pthread_spin_lock(&spinLock);
    if (res != 0) {
        if (res == EBUSY) {
            return DBGUTIL_ERR_RESOURCE_BUSY;
        } else if (res == EINVAL) {
            return DBGUTIL_ERR_INVALID_ARGUMENT;
        } else {
            return DBGUTIL_ERR_SYSTEM_FAILURE;
        }
    }
#endif
    return DBGUTIL_ERR_OK;
}

DbgUtilErr OsUtil::unlockSpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    LeaveCriticalSection(&spinLock);
#else
    int res = pthread_spin_unlock(&spinLock);
    if (res != 0) {
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
#endif
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil
