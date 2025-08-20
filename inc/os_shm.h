#ifndef __OS_SHM_H__
#define __OS_SHM_H__

#include <cstdlib>
#include <list>
#include <string>

#include "dbg_util_def.h"
#include "dbg_util_err.h"

namespace dbgutil {

/** @brief Base class for shared memory segment. */
class DBGUTIL_API OsShm {
public:
    /** @brief Virtual destructor. */
    virtual ~OsShm() {}

    /**
     * @brief Creates a shared memory segment for reading/writing by the given name and size.
     * @param name The shared memory segment name.
     * @param size The shared memory segment size.
     * @param shareWrite Specifies whether the shared memory segment should be opened to allow write
     * access for other processes as well.
     * @return DbgUtilErr The operations's result.
     */
    virtual DbgUtilErr createShm(const char* name, size_t size, bool shareWrite) = 0;

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
    virtual DbgUtilErr openShm(const char* name, size_t size, bool allowWrite = false,
                               bool allowMapBackingFile = false,
                               bool* backingFileMapped = nullptr) = 0;

    /**
     * @brief Synchronizes shared memory segment to backing file (not supported on all platforms).
     */
    virtual DbgUtilErr syncShm() = 0;

    /**
     * @brief Closes the shared memory segment. The shared memory segment is still kept alive in
     * memory though, and if actual deletion is required, then call @ref destroyShm().
     * @return DbgUtilErr The operations's result.
     */
    virtual DbgUtilErr closeShm() = 0;

    /**
     * @brief Queries whether the shared memory object is opened. This will be true after calling
     * either to @ref createShm() or to @ref openShm(). After calling to @ref closeShm() this will
     * return false.
     */
    inline bool isOpen() const { return m_shmPtr != nullptr; }

    /** @brief Retrieves a pointer to the first byte of the shared memory segment. */
    inline void* getShmPtr() { return m_shmPtr; }

    /** @brief Retrieves the shared memory segment name. */
    inline const char* getShmName() const { return m_name.c_str(); }

protected:
    OsShm() : m_size(0), m_shmPtr(nullptr) {}
    OsShm(const OsShm&) = delete;
    OsShm(OsShm&&) = delete;
    OsShm& operator=(const OsShm&) = delete;

    std::string m_name;
    size_t m_size;
    void* m_shmPtr;
};

/** @brief Creates a platform-independent shared memory management object. */
extern DBGUTIL_API OsShm* createOsShm();

}  // namespace dbgutil

#endif  // __OS_SHM_H__