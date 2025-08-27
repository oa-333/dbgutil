#ifndef __WIN32_SHM_H__
#define __WIN32_SHM_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

#include "os_shm.h"

#ifdef DBGUTIL_MINGW
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace dbgutil {

class Win32Shm : public OsShm {
public:
    Win32Shm() : m_backingFile(INVALID_HANDLE_VALUE), m_mapFile(nullptr) {}
    Win32Shm(const Win32Shm&) = delete;
    Win32Shm(Win32Shm&&) = delete;
    Win32Shm& operator=(const Win32Shm&) = delete;
    ~Win32Shm() final {}

    /**
     * @brief Creates a shared memory segment for reading/writing by the given name and size.
     * @param name The shared memory segment name.
     * @param size The shared memory segment size.
     * @param shareWrite Specifies whether the shared memory segment should be opened to allow write
     * access for other processes as well.
     * @return DbgUtilErr The operations's result.
     */
    DbgUtilErr createShm(const char* name, size_t size, bool shareWrite) final;

    /**
     * @brief Opens an existing shared memory for reading segment by the given name and size.
     * @param name The shared memory segment name.
     * @param size The shared memory segment size.
     * @param allowWrite Optionally specifies whether the shared memory segment should be opened
     * also for writing.
     * @param allowMapBackingFile (Windows only) Optionally specifies whether to attempt mapping an
     * existing backing file in case the shared memory segment cannot be opened (i.e. the owning
     * process died and there was no other open handle to the shared memory). In this case, the
     * mapped file and the shared memory segment are opened for read-only. The optional output
     * parameter @ref backingFileMapped can be used to distinguish between the two cases.
     * @param[out] backingFileMapped Optionally on return indicates whether the backing file was
     * mapped or not.
     * @return DbgUtilErr The operations's result.
     */
    DbgUtilErr openShm(const char* name, size_t size, bool allowWrite = false,
                       bool allowMapBackingFile = false, bool* backingFileMapped = nullptr) final;

    /**
     * @brief Synchronizes shared memory segment to backing file (not supported on all platforms).
     */
    DbgUtilErr syncShm() final;

    /**
     * @brief Closes the shared memory segment. The shared memory segment is still kept alive in
     * memory though, and if actual deletion is required, then call @ref destroyShm().
     * @return DbgUtilErr The operations's result.
     */
    DbgUtilErr closeShm() final;

    /**
     * @brief Removes a shared memory segment from the memory. This usually happens after all open
     * mappings, and file descriptors pointing to the shared memory object, are closed.
     * @param name The name used to refer to the shared memory segment. On Windows this should be
     * the name of the backing file. On Linux that would be the name of the shared memory segment.
     * @return DbgUtilErr The operations's result.
     * @note This call does not require the shared memory segment to be opened first.
     * @note If a null or empty string is passed, then the backing file will be used.
     */
    static DbgUtilErr deleteShm(const char* name);

    /** @brief Retrieves the directory where shared memory segment files are found. */
    static const char* getShmPath();

private:
    HANDLE m_backingFile;
    HANDLE m_mapFile;
};

extern DbgUtilErr initWin32Shm();
extern DbgUtilErr termWin32Shm();

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS

#endif  // __WIN32_SHM_H__