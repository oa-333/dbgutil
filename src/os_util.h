#ifndef __OS_UTIL_H__
#define __OS_UTIL_H__

#include <fcntl.h>

#include <ctime>
#include <vector>

#include "dbg_util_def.h"
#include "dbg_util_err.h"

// header required for spin locks
#ifndef DBGUTIL_MSVC
#include <pthread.h>
#endif

// define file IO stuff
#ifdef DBGUTIL_MSVC
// open flags
#define O_CREAT _O_CREAT
#define O_EXCL _O_EXCL
#define O_TRUNC _O_TRUNC
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR _O_RDWR
#define O_BINARY _O_BINARY
#define O_TEXT _O_TEXT

// create mode
#define S_IREAD _S_IREAD
#define S_IWRITE _S_IWRITE
#define S_IEXEC _S_IEXEC

#define _S_IRWXU (S_IREAD | S_IWRITE | S_IEXEC)
#define _S_IXUSR _S_IEXEC
#define _S_IWUSR _S_IWRITE

#define S_IRWXU (S_IREAD | S_IWRITE | S_IEXEC)
#define S_IXUSR S_IEXEC
#define S_IWUSR S_IWRITE
#define S_IRUSR S_IREAD

#define S_IRGRP 0
#define S_IWGRP 0
#define S_IXGRP 0
#define S_IRWXG 0

#define S_IROTH 0
#define S_IWOTH 0
#define S_IXOTH 0
#define S_IRWXO 0

#endif

// resolve once and for all the O_BINARY flag due to MinGW
#ifdef DBGUTIL_GCC
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif

namespace dbgutil {

#ifdef DBGUTIL_MSVC
typedef CRITICAL_SECTION csi_spinlock_t;
#else
typedef pthread_spinlock_t csi_spinlock_t;
#endif

/** @brief Various OS utilities (platform independent). */
class OsUtil {
public:
    static void initLogger();
    static void termLogger();

    /** @brief Retrieves current thread identifier (non-pseudo number). */
    static os_thread_id_t getCurrentThreadId();

    /** @brief Retrieves current UTC time (seconds). */
    inline app_time_t getCurrentTime() {
        time_t now = time(nullptr);
        return (app_time_t)now;
    }

    /**
     * @brief Deletes a file.
     * @param filePath The path of the file to delete. Could be relative or absolute.
     * @return E_OK If the file exists and was deleted.
     * @return E_NOT_FOUND If the file was not found.
     * @return E_INVALID_ARGUMENT If the file path is invalid.
     * @return DbgUtilErr Any other error code.
     */
    static DbgUtilErr deleteFile(const char* filePath);

    /**
     * @brief Queries whether a file exists.
     * @param filePath The file path to query. Could be relative or absolute.
     * @return E_OK If the file exists.
     * @return E_NOT_FOUND If the file was not found.
     * @return E_INVALID_STATE If the file was found but is not a regular file (i.e. a directory or
     * something else).
     * @return E_SYSTEM_FAILURE If a system call failed for any other reason than listed above.
     * @return DbgUtilErr Any other error code.
     */
    static DbgUtilErr fileExists(const char* filePath);

    /**
     * @brief Queries whether a directory exists.
     * @param dirPath The directory path to query. Could be relative or absolute.
     * @return E_OK If the directory exists.
     * @return E_NOT_FOUND If the directory was not found.
     * @return E_INVALID_STATE If the path was found but is not a regular file (i.e. a regular file
     * or something else).
     * @return E_SYSTEM_FAILURE If a system call failed for any other reason than listed above.
     * @return DbgUtilErr Any other error code.
     */
    static DbgUtilErr dirExists(const char* dirPath);

    /**
     * @brief Retrieves the current working directory of the process.
     * @param[out] currentDir The resulting directory path.
     * @return E_OK If the operation succeeded.
     * @return DbgUtilErr Any other error code.
     */
    static DbgUtilErr getCurrentDir(std::string& currentDir);

    /**
     * @brief Create a file system directory.
     * @param path The directory path
     * @return E_OK If the directory was created.
     * @return E_ALREADY_EXISTS If the directory already exists.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr createDir(const char* path);

    /**
     * @brief Deletes a file system directory.
     * @param path The directory path
     * @return E_OK If the directory was deleted.
     * @return E_NOT_FOUND If the directory does not exist.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr deleteDir(const char* path);

    /**
     * @brief Opens a file for I/O.
     * @param path The path to the file
     * @param flags The open flags.
     * @param mode The mode in case the file is created.
     * @param[out] fd The resulting file descriptor.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr openFile(const char* path, int flags, int mode, int& fd);

    /**
     * @brief Closes a file.
     * @param fd The file descriptor.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr closeFile(int fd);

    /**
     * @brief Moves the file pointer to the specified offset.
     * @param fd The file descriptor
     * @param offset The offset (could be negative).
     * @param origin The seek method.
     * @param[out] resultOffset Optional parameter, receiving the resulting offset.
     * @param[out] sysErr Optional parameter, receiving system error code in case of error.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr seekFile(int fd, int64_t offset, int origin, uint64_t* resultOffset = nullptr,
                               int* sysErr = nullptr);

    /**
     * @brief Retrieves current file offset.
     * @param fd The file descriptor
     * @param[out] offset The resulting current file offset.
     * @param[out] sysErr Optional parameter, receiving system error code in case of error.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr getFileOffset(int fd, uint64_t& offset, int* sysErr = nullptr);

    /**
     * @brief Retrieves the size of a file.
     * @param fd The file descriptor.
     * @param[out] fileSizeBytes The resulting file size in bytes.
     * @param[out] sysErr Optional parameter, receiving system error code in case of error.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr getFileSize(int fd, uint64_t& fileSizeBytes, int* sysErr = nullptr);

    /**
     * @brief Writes to an open file.
     * @param fd The file descriptor
     * @param buf The buffer to write.
     * @param len The buffer length.
     * @param[out] bytesWritten The number of bytes actually written.
     * @param[out] sysErr Optional parameter, receiving system error code in case of error.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr writeFile(int fd, const char* buf, size_t len, size_t& bytesWritten,
                                int* sysErr = nullptr);

    /**
     * @brief Reads from an open file.
     * @param fd The file descriptor
     * @param buf The buffer receiving file data.
     * @param len The buffer length.
     * @param[out] bytesRead The number of bytes actually read.
     * @param[out] sysErr Optional parameter, receiving system error code in case of error.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr readFile(int fd, char* buf, size_t len, size_t& bytesRead,
                               int* sysErr = nullptr);

    /**
     * @brief Synchronizes OS file buffers and metadata to disk.
     * @param fd The file descriptor.
     * @param[out] sysErr Optional parameter, receiving system error code in case of error.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr fsyncFile(int fd, int* sysErr = nullptr);

    /**
     * @brief Synchronizes OS file buffers to disk.
     * @param fd The file descriptor.
     * @param[out] sysErr Optional parameter, receiving system error code in case of error.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr fdatasyncFile(int fd, int* sysErr = nullptr);

    /**
     * @brief Reads entire file contents from disk into a raw buffer.
     * @param path The file path.
     * @param buf The buffer.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr readEntireFileToBuf(const char* path, std::vector<char>& buf);

    /**
     * @brief Reads entire file contents from disk into a vector of strings.
     * @param path The file path.
     * @param lines The resulting file lines.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr readEntireFileToLines(const char* path, std::vector<std::string>& lines);

    /**
     * @brief Executes a command in a child process and returns the output.
     * @param cmdLine The command line to execute.
     * @param buf A buffer receiving the output of the child process.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr execCmd(const char* cmdLine, std::vector<char>& buf);

    /**
     * @brief Initializes a spin lock.
     * @param spinLock The spin lock to initialize.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr initializeSpinLock(csi_spinlock_t& spinLock);

    /**
     * @brief Destroys a spin lock object.
     * @param spinLock The spin lock to destroy.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr destroySpinLock(csi_spinlock_t& spinLock);

    /**
     * @brief Locks a spin lock.
     * @param spinLock The spin lock object to lock.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr lockSpinLock(csi_spinlock_t& spinLock);

    /**
     * @brief Attempts to lock a spin lock.
     * @param spinLock The spin lock object to lock.
     * @return E_OK If the lock was acquired.
     * @return E_RESOURCE_BUSY If the lock is held by another thread and cannot be acquired.
     * @return DbgUtilErr Any other error reason.
     */
    static DbgUtilErr tryLockSpinLock(csi_spinlock_t& spinLock);

    /**
     * @brief Unlocks a spin lock.
     * @param spinLock The spin lock object to unlock.
     * @return DbgUtilErr The operation result.
     */
    static DbgUtilErr unlockSpinLock(csi_spinlock_t& spinLock);
};

}  // namespace dbgutil

#endif  // __OS_UTIL_H__