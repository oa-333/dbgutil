#ifndef __LIFE_SIGN_MANAGER_H__
#define __LIFE_SIGN_MANAGER_H__

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <list>
#include <mutex>
#include <string>
#include <vector>

#include "dbg_util_def.h"
#include "dbg_util_err.h"
#include "os_shm.h"

/** @def Maximum path length used in life-sign header. */
#define DBGUTIL_PATH_LEN 256

/** @def Upper bound on the maximum number of threads that can send life-sign reports. */
#define DBGUTIL_MAX_THREADS_UPPER_BOUND 8192

// restrict context area size to 4 MB (which is already way too much)
#define DBGUTIL_MAX_CONTEXT_AREA_SIZE_BYTES (4 * 1024ul * 1024ul)

// restrict life sign area size to 64 MB (more than enough, we only need latest history)
#define DBGUTIL_MAX_LIFE_SIGN_AREA_SIZE_BYTES (64 * 1024ul * 1024ul)

/** @def Restrict the size of a single context record. */
#define DBGUTIL_MAX_CONTEXT_RECORD_SIZE_BYTES (4 * 1024ul)

/** @def Restrict the size of a single life-sign record. */
#define DBGUTIL_MAX_LIFE_SIGN_RECORD_SIZE_BYTES (4 * 1024ul)

namespace dbgutil {

/** @typedef Shared memory segment list. */
typedef std::vector<std::pair<std::string, uint32_t>> ShmSegmentList;

/**
 * @brief Life Sign shared memory main header. This is static data that does not change over time.
 */
struct DBGUTIL_API LifeSignHeader {
    /** @var The image path of the process that produced this life sign segment. */
    char m_imagePath[DBGUTIL_PATH_LEN];

    /** @var The time when the segment was created (local time). */
    int64_t m_startTimeEpochMilliSeconds;

    /** @var The process identifier of the process. */
    uint32_t m_pid;

    /** @var The size in bytes of the context area in the segment. */
    uint32_t m_contextAreaSize;

    /** @var The start offset of the context area (relative to start of segment). */
    uint32_t m_contextAreaStartOffset;

    /** @var The maximum number of threads that send life-sign reports. */
    uint32_t m_maxThreads;

    /** @var The size in bytes of the life-sign area in the segment. */
    uint32_t m_lifeSignAreaSize;

    /** @var The start offset of the life-sign area (relative to start of segment). */
    uint32_t m_lifeSignAreaStartOffset;

    /** @var The size in bytes of the area allocated for each thread (not aligned). */
    uint32_t m_threadAreaSize;

    /** @var Align struct size to 8 bytes. */
    uint32_t m_padding;

// Windows guardian area
#ifdef DBGUTIL_WINDOWS
    /** @brief Last time process was seen alive (local time since epoch, milliseconds). */
    int64_t m_lastProcessTimeEpochMillis;

    /**
     * @brief Last time shared memory segment was synced to disk (local time since epoch,
     * milliseconds).
     */
    int64_t m_lastSyncTimeEpochMillis;

    /** @brief Specifies whether process is still alive (0 - unknown, 1 - alive, 2 - dead). */
    uint32_t m_isProcessAlive;

    /**
     * @brief Specifies whether the shared memory segment was fully synced to disk after the
     * creating process terminated.
     */
    uint32_t m_isFullySynced;
#endif
};

/**
 * @brief The life-sign manager that is used both for storing life-signs by a running process,
 * and for inspecting life-signs after a process crash. The life-sign manager does so through
 * shared memory segments. The shared memory segment allocated for a single process is divided
 * into two main areas - the context area and the life-sign area. The context area is used for
 * storing static context record, so that the life-sign records may be interpreted correctly (or
 * to save space and time while storing life-sign records). The context area is not circular,
 * and if storage area is used up, attempting to store more context records will be rejected.
 * The life-sign area is used to store periodic life-sign records, and is circular, that is old
 * records may be overwritten by new records in case all life-sign area is used up.
 */
class DBGUTIL_API LifeSignManager {
public:
    LifeSignManager(const LifeSignManager&) = delete;
    LifeSignManager(LifeSignManager&&) = delete;
    LifeSignManager& operator=(const LifeSignManager&) = delete;
    virtual ~LifeSignManager() {}

    /**************************************************************************************
     *                              Process Self Life Sign API
     **************************************************************************************/

    /**
     * @brief Create a shared memory segment object of life-sign records for the current process.
     * @param contextAreaSize The size to be allocated for the context area.
     * @param lifeSignAreaSize The size to be allocated for the life-sign area.
     * @param maxThreads The maximum number of threads that can participate in life-sign reports.
     * The life-sign area is divided by this area, so that each thread has a fixed size area for
     * life-sign reports. This number cannot exceed @ref DBGUTIL_MAX_THREADS_UPPER_BOUND.
     * @param shareWrite Specifies whether the shared memory segment should be opened to allow write
     * access for other processes as well.
     * @return DbgUtilErr The operation's result.
     */
    DbgUtilErr createLifeSignShmSegment(uint32_t contextAreaSize, uint32_t lifeSignAreaSize,
                                        uint32_t maxThreads, bool shareWrite);

    /**
     * @brief Opens an existing life-sign shared memory segment for reading.
     * @param segmentName The name of the existing shared memory segment.
     * @param totalSize The total size of the shared memory segment.
     * @param allowWrite Specifies whether the shared memory segment should be opened also for
     * writing.
     * @param allowMapBackingFile (Windows only) Specifies whether to attempt mapping an existing
     * backing file in case the shared memory segment cannot be opened (i.e. the owning process died
     * and there was no other open handle to the shared memory).
     * @return DbgUtilErr The operation's result.
     */
    DbgUtilErr openLifeSignShmSegment(const char* segmentName, uint32_t totalSize, bool allowWrite,
                                      bool allowMapBackingFile = false);

    /**
     * @brief Synchronizes the shared memory segment to backing file (not required/supported on all
     * platforms). Call this occasionally on Windows to ensure the backing file contents are in-sync
     * with the shared memory segment.
     */
    DbgUtilErr syncLifeSignShmSegment();

    /**
     * @brief Closes a previously created/opened shared memory segment.
     * @param deleteShm Optionally specifies whether to delete the segment (by default the segment
     * is not deleted when closing it).
     * @return DbgUtilErr The operation's result.
     */
    DbgUtilErr closeLifeSignShmSegment(bool deleteShm = false);

    /**
     * @brief Deletes a previously created life-sign shared memory segment. This removes the backing
     * file on Windows, and unlinks the segment on POSIX-compliant systems.
     * @param segmentName The name of the existing shared memory segment.
     * @return DbgUtilErr The operation's result.
     */
    virtual DbgUtilErr deleteLifeSignShmSegment(const char* segmentName) = 0;

    /**
     * @brief Writes a context record to the context area (thread-safe).
     * @param recPtr Pointer to the raw context record data to write.
     * @param recLen The context record's length.
     * @return DbgUtilErr The operation's result.
     */
    DbgUtilErr writeContextRecord(const char* recPtr, uint32_t recLen);

    /**
     * @brief Writes a life-sign record to the life-sign area(thread-safe). Blocks temporarily
     * if too many threads are trying to write in parallel to the life-sign area.
     * @param recPtr Pointer to the raw life-sign record data to write.
     * @param recLen The life-sign record's length.
     * @return DbgUtilErr The operation's result.
     */
    DbgUtilErr writeLifeSignRecord(const char* recPtr, uint32_t recLen);

    /**************************************************************************************
     *                              Inspecting Life Sign API
     **************************************************************************************/

    /**
     * @brief Lists all shared memory segments opened by the life-sign manager.
     * @param[out] shmObjects The resulting list of shared memory segments with their size.
     * @return DbgUtilErr The operation's result.
     */
    DbgUtilErr listLifeSignShmSegments(ShmSegmentList& shmObjects);

    /**
     * @brief Reads the life sign header of the shared memory segment.
     * @param[out] hdr Receives a pointer to the shared memory segment header.
     * @return DbgUtilErr The operation's result.
     */
    DbgUtilErr readLifeSignHeader(LifeSignHeader*& hdr);

    /**
     * @brief Reads a context record at the given offset (returns a pointer to shared memory).
     * @param[in,out] offset The offset from start of context area (not from start of segment). When
     * the call returns, it points to the start of the next record.
     * @param[out] recPtr Receive a pointer to the context record.
     * @param[out] recLen The resulting context record length.
     * @return DbgUtilErr The operation's result.
     * @note The caller should start with offset zero, reading context records until
     * DBGUTIL_ERR_END_OF_STREAM is returned.
     */
    DbgUtilErr readContextRecord(uint32_t& offset, char*& recPtr, uint32_t& recLen);

    /**
     * @brief Reads the life sign header of the shared memory segment.
     * @param threadSlotId The thread slot identifier (a number in the range [0, maxThreads-1]).
     * @param[out] threadId The system thread identifier.
     * @param[out] startEpochMilliSeconds The time since epoch when the thread sent its first life
     * sign report.
     * @param[out] endEpochMilliSeconds The time since epoch when the thread finished running.
     * @param[out] isRunning Specified whether the thread was still running by the time the
     * application stopped sending life signs.
     * @param[out] useCount The number of times this slot was used by application threads. If this
     * number is zero then the slot was never used, and therefore it can be skipped.
     * @return DbgUtilErr The operation's result.
     */
    DbgUtilErr readThreadLifeSignDetails(uint32_t threadSlotId, uint64_t& threadId,
                                         int64_t& startEpochMilliSeconds,
                                         int64_t& endEpochMilliSeconds, bool& isRunning,
                                         uint32_t& useCount);

    /**
     * @brief Reads a life-sign record at the given offset. This usually returns a pointer into the
     * shared memory segment, but occasionally it might copy data from shared memory, in which case
     * the caller is responsible for calling @ref releaseLifeSignRecord() when done.
     * @param threadSlotId the slot identifier of the threads, whose life sign records are being
     * read.
     * @param[in,out] offset The offset from start of life-sign area area (not from start of
     * segment). When the call returns, it points to the start of the next record, and it updated in
     * a circular manner. If the offset exceeds the tail position, then end-of-stream is returned.
     * @param[out] recPtr Receive a pointer to the life-sign record. The buffer is allocated
     * internally, and the caller is responsible for calling @ref releaseLifeSignRecord() when done.
     * @param[out] recLen The resulting life-sign record size.
     * @param[out] callerShouldRelease Specifies whether caller should call @ref
     * releaseLifeSignRecord() when done.
     * @return DbgUtilErr The operation's result.
     * @note The caller should start with offset zero, reading context records until
     * DBGUTIL_ERR_END_OF_STREAM is returned.
     */
    DbgUtilErr readLifeSignRecord(uint32_t threadSlotId, uint32_t& offset, char*& recPtr,
                                  uint32_t& recLen, bool& callerShouldRelease);

    /**
     * @brief Releases a life-sign record previously allocated by a call to @ref
     * readLifeSignRecord().
     */
    void releaseLifeSignRecord(char* recPtr);

protected:
    LifeSignManager()
        : m_shm(nullptr),
          m_lifeSignHeader(nullptr),
          m_contextAreaHeader(nullptr),
          m_contextArea(nullptr),
          m_lifeSignArea(nullptr) {}

    virtual DbgUtilErr getImagePath(std::string& imagePath) = 0;
    virtual DbgUtilErr getProcessName(std::string& processName) = 0;
    virtual uint32_t getProcessId() = 0;
    virtual std::string getFileTimeStamp() = 0;
    virtual const char* getShmPath() = 0;
    virtual DbgUtilErr getShmFileSize(const char* shmFilePath, uint32_t& shmSize) = 0;

    DbgUtilErr composeShmName(std::string& shmName);

    /** @brief Creates the shared memory segment. */
    virtual OsShm* createShmObject() = 0;

    /** @brief Destroys the shared memory segment. */
    virtual void destroyShmObject() = 0;

    /** @var The active shared memory segment. */
    OsShm* m_shm;

private:
    struct ContextAreaHeader {
        ContextAreaHeader() : m_writePos(0) {}
        ContextAreaHeader(const ContextAreaHeader&) = delete;
        ContextAreaHeader(ContextAreaHeader&&) = delete;
        ContextAreaHeader& operator=(ContextAreaHeader&) = delete;
        ~ContextAreaHeader() {}

        std::atomic<int32_t> m_writePos;
        uint32_t m_padding;
    };

    struct LifeSignThreadAreaHeader {
        LifeSignThreadAreaHeader()
            : m_threadId(0),
              m_startEpochMilliSeconds(0),
              m_endEpochMilliSeconds(0),
              m_headPos(0),
              m_tailPos(0),
              m_recordCount(0),
              m_state(0) {}
        LifeSignThreadAreaHeader(const LifeSignThreadAreaHeader&) = delete;
        LifeSignThreadAreaHeader(LifeSignThreadAreaHeader&&) = delete;
        LifeSignThreadAreaHeader& operator=(LifeSignThreadAreaHeader&) = delete;
        ~LifeSignThreadAreaHeader() {}

        uint64_t m_threadId;
        int64_t m_startEpochMilliSeconds;
        int64_t m_endEpochMilliSeconds;
        uint32_t m_headPos;
        uint32_t m_tailPos;
        uint32_t m_recordCount;
        uint32_t m_state;
    };

    LifeSignHeader* m_lifeSignHeader;
    ContextAreaHeader* m_contextAreaHeader;
    char* m_contextArea;
    char* m_lifeSignArea;

    std::mutex m_lock;
    std::list<int32_t> m_vacantSlots;

    int32_t obtainThreadSlot();
    void releaseThreadSlot(int32_t slotId);

    friend void cleanupThreadSlot(void*);
};

extern DbgUtilErr initLifeSignManager();
extern DbgUtilErr termLifeSignManager();

/** @brief Retrieves the installed life-sign manager. */
extern DBGUTIL_API LifeSignManager* getLifeSignManager();

}  // namespace dbgutil

#endif  // __LIFE_SIGN_MANAGER_H__