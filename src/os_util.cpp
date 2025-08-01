#include "dbg_util_def.h"

#ifdef DBGUTIL_MINGW
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cinttypes>
#include <climits>
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
#include <share.h>
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

namespace libdbg {

static Logger sLogger;

void OsUtil::initLogger() { registerLogger(sLogger, "os_util"); }
void OsUtil::termLogger() { unregisterLogger(sLogger); }

os_thread_id_t OsUtil::getCurrentThreadId() {
#ifdef DBGUTIL_WINDOWS
    return GetCurrentThreadId();
#else
    return gettid();
#endif
}

LibDbgErr OsUtil::deleteFile(const char* filePath) {
#ifdef DBGUTIL_WINDOWS
    if (!DeleteFileA(filePath)) {
        LOG_WIN32_ERROR(sLogger, DeleteFile, "Failed to delete file %s", filePath);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
#else
    if (unlink(filePath) == -1) {
        LOG_SYS_ERROR(sLogger, unlink, "Failed to delete file %s", filePath);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr OsUtil::fileExists(const char* filePath) {
#ifdef DBGUTIL_WINDOWS
    DWORD dwAttrib = GetFileAttributesA(filePath);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        DWORD rc = GetLastError();
        if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) {
            return LIBDBG_ERR_NOT_FOUND;
        }
        LOG_WIN32_ERROR_NUM(sLogger, GetFileAttributes, rc, "Failed to check file %s existence",
                            filePath);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    if (!(dwAttrib & FILE_ATTRIBUTE_NORMAL) && !(dwAttrib & FILE_ATTRIBUTE_ARCHIVE)) {
        return LIBDBG_ERR_INVALID_STATE;
    }
    return LIBDBG_ERR_OK;
#else
    struct stat buf = {};
    if (lstat(filePath, &buf) != 0) {
        int rc = errno;
        if (rc == ENOENT) {
            return LIBDBG_ERR_NOT_FOUND;
        }
        LOG_SYS_ERROR_NUM(sLogger, lstat, rc, "Failed to check file %s existence", filePath);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    if (!S_ISREG(buf.st_mode)) {
        return LIBDBG_ERR_INVALID_STATE;
    }
    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr OsUtil::dirExists(const char* dirPath) {
#ifdef DBGUTIL_WINDOWS
    DWORD dwAttrib = GetFileAttributesA(dirPath);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        DWORD rc = GetLastError();
        if (rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND) {
            return LIBDBG_ERR_NOT_FOUND;
        }
        LOG_WIN32_ERROR_NUM(sLogger, GetFileAttributes, rc, "Failed to check file %s existence",
                            dirPath);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    if (!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return LIBDBG_ERR_INVALID_STATE;
    }
    return LIBDBG_ERR_OK;
#else
    struct stat buf = {};
    if (lstat(dirPath, &buf) != 0) {
        int rc = errno;
        if (rc == ENOENT) {
            return LIBDBG_ERR_NOT_FOUND;
        }
        LOG_SYS_ERROR_NUM(sLogger, lstat, rc, "Failed to check file %s existence", dirPath);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    if (!S_ISDIR(buf.st_mode)) {
        return LIBDBG_ERR_INVALID_STATE;
    }
    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr OsUtil::getCurrentDir(std::string& currentDir) {
#ifdef DBGUTIL_WINDOWS
    DWORD len = GetCurrentDirectoryA(0, nullptr);
    if (len == 0) {
        LOG_WIN32_ERROR(sLogger, GetCurrentDirectoryA,
                        "Failed to retrieve current working directory");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    char* buf = (char*)malloc(len + 1);
    if (buf == nullptr) {
        LOG_SYS_ERROR(sLogger, malloc,
                      "Failed to allocate %u bytes for retrieving current directory", len);
        return LIBDBG_ERR_NOMEM;
    }
    if (GetCurrentDirectoryA(len + 1, buf) == 0) {
        LOG_WIN32_ERROR(sLogger, GetCurrentDirectoryA,
                        "Failed to retrieve current working directory");
        free(buf);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    currentDir = buf;
    free(buf);
    buf = nullptr;
    return LIBDBG_ERR_OK;
#else
    char buf[PATH_MAX];
    if (getcwd(buf, PATH_MAX) == nullptr) {
        LOG_SYS_ERROR(sLogger, getcwd, "Failed to retrieve current working directory");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    currentDir = buf;
    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr OsUtil::createDir(const char* path) {
#ifdef DBGUTIL_WINDOWS
    if (!CreateDirectoryA(path, nullptr)) {
        DWORD rc = GetLastError();
        if (rc == ERROR_ALREADY_EXISTS) {
            return LIBDBG_ERR_ALREADY_EXISTS;
        }
        LOG_WIN32_ERROR(sLogger, CreateDirectoryA, "Failed to create directory: %s", path);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
#else
    if (mkdir(path, 0700) != 0) {
        int rc = errno;
        if (rc == EEXIST) {
            return LIBDBG_ERR_ALREADY_EXISTS;
        }
        LOG_SYS_ERROR(sLogger, mkdir, "Failed to create directory: %s", path);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
#endif
}

// TODO: add utility to map system error code to internal error code or default to E_SYSTEM_FAILURE.
LibDbgErr OsUtil::deleteDir(const char* path) {
#ifdef DBGUTIL_WINDOWS
    if (!RemoveDirectoryA(path)) {
        DWORD rc = GetLastError();
        if (rc == ERROR_NOT_FOUND) {
            return LIBDBG_ERR_NOT_FOUND;
        }
        LOG_WIN32_ERROR(sLogger, CreateDirectoryA, "Failed to delete directory: %s", path);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
#else
    if (rmdir(path) != 0) {
        int rc = errno;
        if (rc == ENOENT) {
            return LIBDBG_ERR_NOT_FOUND;
        }
        LOG_SYS_ERROR(sLogger, mkdir, "Failed to delete directory: %s", path);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr OsUtil::openFile(const char* path, int flags, int mode, int& fd) {
#ifdef DBGUTIL_WINDOWS
    errno_t errc = _sopen_s(&fd, path, flags, _SH_DENYNO, mode);
    if (errc != 0) {
        LOG_SYS_ERROR(sLogger, _sopen_s, "Failed to open file: %s", path);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
#else
    fd = open(path, flags, mode);
    if (fd == -1) {
        LOG_SYS_ERROR(sLogger, open, "Failed to open file: %s", path);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::closeFile(int fd) {
#ifdef DBGUTIL_WINDOWS
    int res = _close(fd);
#else
    int res = close(fd);
#endif
    if (res == -1) {
        LOG_SYS_ERROR(sLogger, close, "Failed to close file descriptor");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::seekFile(int fd, int64_t offset, int origin,
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
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    if (resultOffset != nullptr) {
        if (res < 0) {
            LOG_ERROR(sLogger, "Unexpected negative file offset returned from lseek64(): %" PRId64,
                      res);
            return LIBDBG_ERR_SYSTEM_FAILURE;
        }
        *resultOffset = (uint64_t)res;
    }
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::getFileOffset(int fd, uint64_t& offset, int* sysErr /* = nullptr */) {
    return seekFile(fd, 0, SEEK_CUR, &offset, sysErr);
}

LibDbgErr OsUtil::getFileSize(int fd, uint64_t& fileSizeBytes, int* sysErr /* = nullptr */) {
    // first save current offset
    uint64_t currOffset = 0;
    LibDbgErr rc = getFileOffset(fd, currOffset, sysErr);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }

    // now seek to end
    rc = seekFile(fd, 0, SEEK_END, &fileSizeBytes, sysErr);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }

    // restore previous offset
    return seekFile(fd, (int64_t)currOffset, SEEK_SET, nullptr, sysErr);
}

LibDbgErr OsUtil::writeFile(int fd, const char* buf, size_t len, size_t& bytesWritten,
                            int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    // on Windows, _write() receives unsigned int as buffer length, while on Unix/Linux it is
    // size_t. Since the len parameter is size_t (due to Unix/Linux), we must test that the value
    // does not exceed unsigned integer maximum value.
    if (len >= UINT32_MAX) {
        LOG_ERROR(sLogger, "Buffer size %zu too large to write to file", len);
        return LIBDBG_ERR_INVALID_ARGUMENT;
    }
    int res = _write(fd, buf, (unsigned int)len);
#else
    ssize_t res = write(fd, buf, len);
#endif
    if (res == -1) {
        if (sysErr != nullptr) {
            *sysErr = errno;
        }
        // refrain from writing to log, this may cause log flooding in case of intense writing
        // LOG_SYS_ERROR(sLogger, write, "Failed to write to file");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    bytesWritten = (size_t)res;
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::readFile(int fd, char* buf, size_t len, size_t& bytesRead,
                           int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    // on Windows, _read() receives unsigned int as buffer length, while on Unix/Linux it is
    // size_t. Since the len parameter is size_t (due to Unix/Linux), we must test that the value
    // does not exceed unsigned integer maximum value.
    if (len >= UINT32_MAX) {
        LOG_ERROR(sLogger, "Buffer size %zu too large to write to file", len);
        return LIBDBG_ERR_INVALID_ARGUMENT;
    }
    int res = _read(fd, buf, (unsigned int)len);
#else
    ssize_t res = read(fd, buf, len);
#endif
    if (res == -1) {
        if (sysErr != nullptr) {
            *sysErr = errno;
        }
        // refrain from writing to log, this may cause log flooding in case of intense reading
        // LOG_SYS_ERROR(sLogger, write, "Failed to read from file");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    bytesRead = (size_t)res;
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::fsyncFile(int fd, int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    LibDbgErr rc = LIBDBG_ERR_OK;
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        rc = LIBDBG_ERR_INVALID_ARGUMENT;
    } else {
        if (!FlushFileBuffers(handle)) {
            // try to translate some Windows errors into rough approximations of POSIX errors
            DWORD err = GetLastError();
            switch (err) {
                case ERROR_ACCESS_DENIED:
                    // For a read-only handle, fsync should succeed, even though we have no way to
                    // sync the access-time changes
                    errno = EACCES;
                    rc = LIBDBG_ERR_ACCESS_DENIED;
                    break;

                case ERROR_INVALID_HANDLE:
                    // probably trying to fsync a tty
                    errno = EINVAL;
                    rc = LIBDBG_ERR_INVALID_ARGUMENT;
                    break;

                default:
                    errno = EIO;
                    rc = LIBDBG_ERR_SYSTEM_FAILURE;
                    break;
            }
        }
    }

    if (rc != LIBDBG_ERR_OK && sysErr != nullptr) {
        *sysErr = errno;
    }
    return rc;
#else
    if (fsync(fd) == -1) {
        if (sysErr != nullptr) {
            *sysErr = errno;
        }
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr OsUtil::fdatasyncFile(int fd, int* sysErr /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    // TODO: implement this
#ifdef SIM_FDATASYNC
    return fsyncFile(fd, sysErr);
#else
    LibDbgErr rc = LIBDBG_ERR_OK;
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        rc = LIBDBG_ERR_INVALID_ARGUMENT;
    } else {
        if (initialize_ntdll() < 0) {
            errno = EFAULT;  //  TODO: is this meaningful?
            return LIBDBG_ERR_SYSTEM_FAILURE;
        }

        IO_STATUS_BLOCK ioStatBlock = {};
        memset(&ioStatBlock, 0, sizeof(&ioStatBlock));
        NTSTATUS status =
            NtFlushBuffersFileEx(handle, FLUSH_FLAGS_FILE_DATA_SYNC_ONLY, NULL, 0, &ioStatBlock);
        if (NT_SUCCESS(status)) {
            return LIBDBG_ERR_OK;
        }

        // TODO: this should be fixed
        ULONG errCode = RtlNtStatusToDosError(status);
        SetLastError(errCode);
        rc = LIBDBG_ERR_SYSTEM_FAILURE;
        errno = EFAULT;  //  TODO: is this meaningful?
        /*const std::error_condition errCond =
            std::system_category().default_error_condition(errCode);
        return errCond.value();*/
    }

    if (rc != LIBDBG_ERR_OK && sysErr != nullptr) {
        *sysErr = errno;
    }
    return rc;
#endif
#else
    if (fsync(fd) == -1) {
        if (sysErr != nullptr) {
            *sysErr = errno;
        }
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return LIBDBG_ERR_OK;
#endif
}

LibDbgErr OsUtil::readEntireFileToBuf(const char* path, std::vector<char>& buf) {
    int fd = 0;
    LibDbgErr rc = openFile(path, O_BINARY | O_RDONLY, 0, fd);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }

    // some files cannot be seek-ed, such as /proc files, so we just read in chunks of 4K
    const int BUF_INC_SIZE = 4096;
    size_t offset = 0;
    size_t bytesRead = 0;
    do {
        if (buf.size() - offset == 0) {
            buf.resize(buf.size() + BUF_INC_SIZE);
        }
        rc = readFile(fd, &buf[offset], buf.size() - offset, bytesRead);
        if (rc != LIBDBG_ERR_OK) {
            return rc;
        }
        offset += bytesRead;
    } while (bytesRead != 0);
    // NOTE: reading less than requested does not necessarily indicate end of file, according to man
    // page only return value of 0 truly indicates end of file

    return closeFile(fd);
}

LibDbgErr OsUtil::readEntireFileToLines(const char* path, std::vector<std::string>& lines) {
    std::vector<char> buf;
    LibDbgErr rc = readEntireFileToBuf(path, buf);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }

    // break to lines
    size_t prevIdx = 0;  // always one past previous line
    do {
        size_t idx = 0;
        std::vector<char>::iterator itr =
            std::find(buf.begin() + (int64_t)prevIdx, buf.end(), '\n');
        if (itr == buf.end()) {
            idx = buf.size();
        } else {
            idx = (size_t)(itr - buf.begin());
        }

        std::string line(&buf[prevIdx], idx - prevIdx);
        lines.push_back(line);
        prevIdx = idx + 1;
    } while (prevIdx < buf.size());
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::execCmd(const char* cmdLine, std::vector<char>& buf) {
#ifdef DBGUTIL_WINDOWS
    FILE* output = _popen(cmdLine, "r");
#else
    FILE* output = popen(cmdLine, "r");
#endif
    if (output == nullptr) {
        LOG_SYS_ERROR(sLogger, popen, "Failed to execute command: %s", cmdLine);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }

    const size_t BUF_SIZE = 4096;
    const size_t LOW_WATERMARK = 1024;
    const size_t INC_FACTOR = 2;
    buf.resize(BUF_SIZE);
    size_t pos = 0;
    size_t bufSize = BUF_SIZE;
    int bufSizeInt = BUF_SIZE;
    LibDbgErr copyRes = LIBDBG_ERR_OK;
    // fgets reads at most bufSize-1 puts a terminating null
    // newline chars are copied as is
    while (fgets(&buf[pos], bufSizeInt, output)) {
        pos += strlen(&buf[pos]);
        if (buf.size() - pos < LOW_WATERMARK) {
            buf.resize(buf.size() * INC_FACTOR);
        }
        bufSize = buf.size() - pos;
        if (bufSize >= INT32_MAX) {
            LOG_ERROR(sLogger, "Invalid buffer size %zu when executing command %s", bufSize,
                      cmdLine);
            copyRes = LIBDBG_ERR_INTERNAL_ERROR;
            break;
        }
        bufSizeInt = (int)bufSize;
    }

    int isEof = feof(output);
#ifdef DBGUTIL_WINDOWS
    int res = _pclose(output);
#else
    int res = pclose(output);
#endif
    if (res != 0) {
        LOG_ERROR(sLogger, "Cmd line execution finished with error %d", res);
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    if (!isEof) {
        LOG_ERROR(sLogger, "Failed to read command line output till the end");
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
    return copyRes;
}

LibDbgErr OsUtil::initializeSpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    InitializeCriticalSection(&spinLock);
#else
    int res = pthread_spin_init(&spinLock, 0);
    if (res != 0) {
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::destroySpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    DeleteCriticalSection(&spinLock);
#else
    int res = pthread_spin_destroy(&spinLock);
    if (res != 0) {
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::lockSpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    EnterCriticalSection(&spinLock);
#else
    int res = pthread_spin_lock(&spinLock);
    if (res != 0) {
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::tryLockSpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    if (!TryEnterCriticalSection(&spinLock)) {
        return LIBDBG_ERR_RESOURCE_BUSY;
    }
#else
    int res = pthread_spin_lock(&spinLock);
    if (res != 0) {
        if (res == EBUSY) {
            return LIBDBG_ERR_RESOURCE_BUSY;
        } else if (res == EINVAL) {
            return LIBDBG_ERR_INVALID_ARGUMENT;
        } else {
            return LIBDBG_ERR_SYSTEM_FAILURE;
        }
    }
#endif
    return LIBDBG_ERR_OK;
}

LibDbgErr OsUtil::unlockSpinLock(csi_spinlock_t& spinLock) {
#ifdef DBGUTIL_MSVC
    LeaveCriticalSection(&spinLock);
#else
    int res = pthread_spin_unlock(&spinLock);
    if (res != 0) {
        return LIBDBG_ERR_SYSTEM_FAILURE;
    }
#endif
    return LIBDBG_ERR_OK;
}

}  // namespace libdbg
