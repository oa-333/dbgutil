#include "life_sign_manager.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <regex>
#include <sstream>

#include "dbgutil_log_imp.h"
#include "dbgutil_tls.h"
#include "dir_scanner.h"
#include "life_sign_manager_internal.h"

#define DBGUTIL_SHM_PREFIX "dbgutil.life-sign"
#define DBGUTIL_SHM_SUFFIX "shm"

#define DBGUTIL_INVALID_THREAD_SLOT_ID ((int32_t)-1)
#define DBGUTIL_NO_THREAD_SLOT_ID ((int32_t)-2)

#define ALIGN(size, align) (((size) + (align) - 1) / (align) * (align))
#define ALIGN_SIZE_BYTES 4

namespace dbgutil {

static Logger sLogger;

static LifeSignManager* sLifeSignManager = nullptr;

static TlsKey sThreadSlotKey = DBGUTIL_INVALID_TLS_KEY;
static thread_local int32_t sThreadSlotId = DBGUTIL_INVALID_THREAD_SLOT_ID;

inline int64_t getEpochTimeMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void cleanupThreadSlot(void* key) {
    int32_t slotId = (int32_t)(uint64_t)key;
    // NOTE: slot id is saved offset by, so that it would not be equal to zero, and then cleanup
    // function will not be called
    --slotId;

    getLifeSignManager()->releaseThreadSlot(slotId);
}

DbgUtilErr LifeSignManager::createLifeSignShmSegment(uint32_t contextAreaSize,
                                                     uint32_t lifeSignAreaSize, uint32_t maxThreads,
                                                     bool shareWrite) {
    // check arguments
    if (contextAreaSize > DBGUTIL_MAX_CONTEXT_AREA_SIZE_BYTES) {
        LOG_ERROR(sLogger,
                  "Cannot create life sign manager, context area size of %zu bytes exceeds allowed "
                  "maximum of %u bytes",
                  contextAreaSize, DBGUTIL_MAX_CONTEXT_AREA_SIZE_BYTES);
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }
    if (lifeSignAreaSize > DBGUTIL_MAX_LIFE_SIGN_AREA_SIZE_BYTES) {
        LOG_ERROR(
            sLogger,
            "Cannot create life sign manager, life-sign area size of %zu bytes exceeds allowed "
            "maximum of %u bytes",
            lifeSignAreaSize, DBGUTIL_MAX_LIFE_SIGN_AREA_SIZE_BYTES);
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }
    if (maxThreads > DBGUTIL_MAX_THREADS_UPPER_BOUND) {
        LOG_ERROR(sLogger,
                  "Cannot create life-sign manager, maximum number of threads %u exceeds allowed "
                  "maximum of %u threads",
                  maxThreads, (unsigned)DBGUTIL_MAX_THREADS_UPPER_BOUND);
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    // check state
    if (m_shm != nullptr) {
        LOG_ERROR(sLogger, "Cannot create life-sign shared memory segment, already created");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // create TLS key (for slot cleanup)
    if (!createTls(sThreadSlotKey, cleanupThreadSlot)) {
        LOG_ERROR(sLogger,
                  "Cannot create life-sign shared memory segment, failed to allocate TLS key for "
                  "thread slot cleanup");
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // compose name
    std::string shmName;
    DbgUtilErr rc = composeShmName(shmName);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to compose shared memory segment name");
        destroyTls(sThreadSlotKey);
        sThreadSlotKey = DBGUTIL_INVALID_TLS_KEY;
        return rc;
    }

    // create shared memory object
    m_shm = createShmObject();
    if (m_shm == nullptr) {
        LOG_ERROR(sLogger, "Failed to create shared memory object, out of memory");
        destroyTls(sThreadSlotKey);
        sThreadSlotKey = DBGUTIL_INVALID_TLS_KEY;
        return DBGUTIL_ERR_NOMEM;
    }

    // create shared memory segment
    uint32_t shmSize = sizeof(LifeSignHeader) + contextAreaSize + lifeSignAreaSize;
    rc = m_shm->createShm(shmName.c_str(), shmSize, shareWrite);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to create shared memory segment by name %s, with total size %zu",
                  shmName.c_str(), shmSize);
        destroyShmObject();
        m_shm = nullptr;
        destroyTls(sThreadSlotKey);
        sThreadSlotKey = DBGUTIL_INVALID_TLS_KEY;
        return rc;
    }

    std::string imagePath;
    getImagePath(imagePath);

    // initialize segment header
    void* shmPtr = m_shm->getShmPtr();
    m_lifeSignHeader = (LifeSignHeader*)shmPtr;
    // copy terminating null as well
    size_t charsToCopy = imagePath.length() + 1;
    size_t charsCanCopy = std::min(charsToCopy, (size_t)DBGUTIL_PATH_LEN);
#ifdef DBGUTIL_WINDOWS
    errno_t errc =
        strncpy_s(m_lifeSignHeader->m_imagePath, DBGUTIL_PATH_LEN, imagePath.c_str(), charsCanCopy);
    if (errc != 0) {
        LOG_SYS_ERROR_NUM(
            sLogger, strncpy_s, errc,
            "Failed to copy image path %s to shared memory segment header, buffer overflow",
            imagePath.c_str());
    }
#else
    strncpy(m_lifeSignHeader->m_imagePath, imagePath.c_str(), charsCanCopy);
#endif
    m_lifeSignHeader->m_imagePath[DBGUTIL_PATH_LEN - 1] = 0;
    m_lifeSignHeader->m_startTimeEpochMilliSeconds = getEpochTimeMilliseconds();
    m_lifeSignHeader->m_pid = getProcessId();
    m_lifeSignHeader->m_contextAreaSize = contextAreaSize;
    m_lifeSignHeader->m_contextAreaStartOffset = sizeof(LifeSignHeader);
    m_lifeSignHeader->m_lifeSignAreaSize = lifeSignAreaSize;
    m_lifeSignHeader->m_lifeSignAreaStartOffset = sizeof(LifeSignHeader) + contextAreaSize;
    m_lifeSignHeader->m_maxThreads = maxThreads;
    uint32_t threadAreaSize = lifeSignAreaSize / maxThreads;
    m_lifeSignHeader->m_threadAreaSize = ALIGN(threadAreaSize, ALIGN_SIZE_BYTES);

    // create atomic variable for context record concurrency (use placement new)
    void* contextArea = (void*)(((char*)shmPtr) + m_lifeSignHeader->m_contextAreaStartOffset);
    m_contextAreaHeader = new (contextArea) ContextAreaHeader();
    m_contextArea = (char*)(m_contextAreaHeader + 1);

    // create life sign thread area
    m_lifeSignArea = (((char*)shmPtr) + m_lifeSignHeader->m_lifeSignAreaStartOffset);
    char* threadOffset = (char*)m_lifeSignArea;
    for (uint32_t i = 0; i < maxThreads; ++i) {
        // initialize header
        new (threadOffset) LifeSignThreadAreaHeader();
        threadOffset += m_lifeSignHeader->m_threadAreaSize;
        // NOTE: cast is ok due to DBGUTIL_MAX_THREADS_UPPER_BOUND
        m_vacantSlots.push_back((int32_t)i);
    }

// synchronize to disk all initial data
#ifdef DBGTUIL_WINDOWS
    m_lifeSignHeader->m_lastProcessTimeEpochMillis = 0;
    m_lifeSignHeader->m_lastSyncTimeEpochMillis = 0;
    m_lifeSignHeader->m_isProcessAlive = 1;
    m_lifeSignHeader->m_isFullySynced = 0;
    rc = m_shm->syncShm();
    if (rc != DBGUTIL_ERR_OK) {
        LOG_WARN(sLogger, "Failed to synchronize shared memory segment to disk (error code: %d)",
                 rc);
    }
#endif

    return DBGUTIL_ERR_OK;
}

DbgUtilErr LifeSignManager::openLifeSignShmSegment(const char* segmentName, uint32_t totalSize,
                                                   bool allowWrite,
                                                   bool allowMapBackingFile /* = false */) {
    // check state
    if (m_shm != nullptr) {
        LOG_ERROR(sLogger, "Cannot open life-sign shared memory segment, already created");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // create shared memory object
    m_shm = createShmObject();
    if (m_shm == nullptr) {
        LOG_ERROR(sLogger, "Failed to create shared memory object, out of memory");
        return DBGUTIL_ERR_NOMEM;
    }

    // open shared memory segment
    DbgUtilErr rc = m_shm->openShm(segmentName, totalSize, allowWrite, allowMapBackingFile);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to open shared memory segment by name %s, with total size %u",
                  segmentName, totalSize);
        destroyShmObject();
        m_shm = nullptr;
        return rc;
    }

    // get headers
    void* shmPtr = m_shm->getShmPtr();
    m_lifeSignHeader = (LifeSignHeader*)shmPtr;
    m_contextAreaHeader =
        (ContextAreaHeader*)(((char*)shmPtr) + m_lifeSignHeader->m_contextAreaStartOffset);
    m_contextArea = (char*)(m_contextAreaHeader + 1);
    m_lifeSignArea = (((char*)shmPtr) + m_lifeSignHeader->m_lifeSignAreaStartOffset);
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LifeSignManager::syncLifeSignShmSegment() {
    if (m_shm == nullptr) {
        LOG_ERROR(sLogger,
                  "Cannot synchronize life-sign shared memory segment to disk, not opened");
        return DBGUTIL_ERR_INVALID_STATE;
    }
    DbgUtilErr rc = m_shm->syncShm();
    if (rc == DBGUTIL_ERR_OK) {
#ifdef DBGUTIL_WINDOWS
        m_lifeSignHeader->m_lastSyncTimeEpochMillis = getEpochTimeMilliseconds();
#endif
    }
    return rc;
}

DbgUtilErr LifeSignManager::closeLifeSignShmSegment(bool deleteShm /* = false */) {
    if (m_shm == nullptr) {
        LOG_ERROR(sLogger, "Cannot close life-sign shared memory segment, not opened");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    std::string name = m_shm->getShmName();
    DbgUtilErr rc = m_shm->closeShm();
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to close life-sign shared memory segment");
        return rc;
    }
    destroyShmObject();
    m_shm = nullptr;
    m_lifeSignHeader = nullptr;
    m_contextAreaHeader = nullptr;
    m_contextArea = nullptr;
    m_lifeSignArea = nullptr;

    if (sThreadSlotKey != DBGUTIL_INVALID_TLS_KEY) {
        destroyTls(sThreadSlotKey);
        sThreadSlotKey = DBGUTIL_INVALID_TLS_KEY;
    }

    if (deleteShm) {
        rc = deleteLifeSignShmSegment(name.c_str());
        if (rc != DBGUTIL_ERR_OK) {
            LOG_ERROR(sLogger, "Failed to delete life-sign shared memory segment by name %s",
                      name.c_str());
            return rc;
        }
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr LifeSignManager::writeContextRecord(const char* recPtr, uint32_t recLen) {
    if (recPtr == nullptr) {
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }
    if (m_contextArea == nullptr) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    if (recLen > DBGUTIL_MAX_CONTEXT_AREA_SIZE_BYTES) {
        return DBGUTIL_ERR_NOMEM;
    }
    uint32_t realLength = recLen + sizeof(uint32_t);
    uint32_t offset = (uint32_t)m_contextAreaHeader->m_writePos.fetch_add(
        (int32_t)realLength, std::memory_order_relaxed);
    if (offset + realLength <= m_lifeSignHeader->m_contextAreaSize) {
        *(uint32_t*)(m_contextArea + offset) = recLen;
        offset += sizeof(uint32_t);
        memcpy(m_contextArea + offset, recPtr, recLen);
        return DBGUTIL_ERR_OK;
    }

    // either context area is full or we lost the race, in any case back off
    m_contextAreaHeader->m_writePos.fetch_add(-((int32_t)realLength), std::memory_order_relaxed);
    return DBGUTIL_ERR_RESOURCE_LIMIT;
}

DbgUtilErr LifeSignManager::writeLifeSignRecord(const char* recPtr, uint32_t recLen) {
    // obtain thread slot
    if (sThreadSlotId == DBGUTIL_INVALID_THREAD_SLOT_ID) {
        sThreadSlotId = obtainThreadSlot();
        if (sThreadSlotId == DBGUTIL_INVALID_THREAD_SLOT_ID) {
            LOG_ERROR(sLogger,
                      "Cannot write life-sign record, cannot obtain slot for current thread, all "
                      "slots are used");
            sThreadSlotId = DBGUTIL_INVALID_THREAD_SLOT_ID;
            return DBGUTIL_ERR_RESOURCE_LIMIT;
        }
    }

    // if already tried once we back off
    if (sThreadSlotId == DBGUTIL_INVALID_THREAD_SLOT_ID) {
        return DBGUTIL_ERR_RESOURCE_LIMIT;
    }

    // with the slot we can get the thread area's base offset
    uint32_t threadAreaSize = m_lifeSignHeader->m_threadAreaSize;
    LifeSignThreadAreaHeader* hdr =
        (LifeSignThreadAreaHeader*)(m_lifeSignArea + sThreadSlotId * threadAreaSize);
    char* threadArea = (char*)(hdr + 1);

    // in order to avoid cutting record length in the middle if wrapping around, we align record
    // length to 4 bytes
    // NOTE: we must reserve space also for terminating null
    uint32_t recLenAligned = ALIGN(recLen + 1, ALIGN_SIZE_BYTES);

    // now we can check record length
    uint32_t realLength = recLenAligned + sizeof(uint32_t);
    if (realLength > threadAreaSize) {
        return DBGUTIL_ERR_NOMEM;
    }

    // no race condition here, just write in a ring buffer
    // NOTE: tail pos and head pos are cyclic
    // the logic here is: while tail + rec still steps over head record, advance head to next record
    uint32_t cyclicDiff = (hdr->m_tailPos + threadAreaSize - hdr->m_headPos) % threadAreaSize;
    while (cyclicDiff + realLength > threadAreaSize) {
        uint32_t headRecLen = *(uint32_t*)(threadArea + hdr->m_headPos);
        hdr->m_headPos += headRecLen + sizeof(uint32_t);
        cyclicDiff = (hdr->m_tailPos + threadAreaSize - hdr->m_headPos) % threadAreaSize;
        --hdr->m_recordCount;
    }

    // now write record, wrap around if needed

    // write real length, not aligned length
    // NOTE: since record length is aligned to 4, we must have space in multiples of 4, so there
    // must be room for at least record length before we wrap around
    assert(hdr->m_tailPos + sizeof(uint32_t) < threadAreaSize);
    *(uint32_t*)(threadArea + hdr->m_tailPos) = recLen + 1;
    hdr->m_tailPos = (hdr->m_tailPos + sizeof(uint32_t)) % threadAreaSize;  // may wrap around

    // check if we need to write record in parts
    uint32_t recEndOffset = hdr->m_tailPos + realLength;
    if (recEndOffset < threadAreaSize) {
        memcpy(threadArea + hdr->m_tailPos, recPtr, recLen);
        *(threadArea + hdr->m_tailPos + recLen) = 0;
    } else {
        uint32_t bytesTillEnd = threadAreaSize - hdr->m_tailPos;
        uint32_t bytesFromStart = recLen + 1 - bytesTillEnd;
        memcpy(threadArea + hdr->m_tailPos, recPtr, bytesTillEnd);
        memcpy(threadArea, recPtr + bytesTillEnd, bytesFromStart);
        *(threadArea + bytesFromStart) = 0;
    }

    // NOTE: advancing by aligned length (original length precedes record)
    hdr->m_tailPos = (hdr->m_tailPos + recLenAligned) % threadAreaSize;
    ++hdr->m_recordCount;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LifeSignManager::listLifeSignShmSegments(ShmSegmentList& shmObjects) {
    // list all files shared memory segment directory
    const char* shmPath = getShmPath();
    std::vector<std::string> fileNames;
    DbgUtilErr rc = DirScanner::scanDirFiles(shmPath, fileNames);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to search for shared memory segments at %s", shmPath);
        return rc;
    }

    // take only those file tha match the name pattern of dbgutil life-sign segments
    std::stringstream s;
    s << "^" << DBGUTIL_SHM_PREFIX << ".*" << DBGUTIL_SHM_SUFFIX << "$";
    std::string shmNamePattern = s.str();
    std::regex pattern(shmNamePattern);
#if __cplusplus >= 202002L
    std::erase_if(fileNames, [&pattern](const std::string& fileName) {
        return !std::regex_match(fileName, pattern);
    });
#else
    std::vector<std::string>::iterator itr = std::remove_if(
        fileNames.begin(), fileNames.end(),
        [&pattern](const std::string& fileName) { return !std::regex_match(fileName, pattern); });
    fileNames.erase(itr, fileNames.end());
#endif

    // get size from backing file
    for (const std::string& fileName : fileNames) {
        std::string filePath = std::string(shmPath) + fileName;
        uint32_t fileSize = 0;
        rc = getShmFileSize(filePath.c_str(), fileSize);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        // we return the file name
        shmObjects.push_back({fileName, fileSize});
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LifeSignManager::readLifeSignHeader(LifeSignHeader*& hdr) {
    // check state
    if (m_lifeSignHeader == nullptr) {
        LOG_ERROR(sLogger, "Cannot read context record, shared segment not open");
        return DBGUTIL_ERR_INVALID_STATE;
    }
    hdr = m_lifeSignHeader;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LifeSignManager::readContextRecord(uint32_t& offset, char*& recPtr, uint32_t& recLen) {
    // check state
    if (m_lifeSignHeader == nullptr) {
        LOG_ERROR(sLogger, "Cannot read context record, shared segment not open");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // check thar record does not exceed context area bounds
    int32_t writePosSigned = m_contextAreaHeader->m_writePos.load(std::memory_order_relaxed);
    if (writePosSigned < 0) {
        LOG_ERROR(sLogger, "Invalid context area header, write pos is negative: %u",
                  writePosSigned);
        return DBGUTIL_ERR_DATA_CORRUPT;
    }
    uint32_t writePos = (uint32_t)writePosSigned;
    if (writePos > DBGUTIL_MAX_CONTEXT_AREA_SIZE_BYTES) {
        LOG_ERROR(sLogger, "Invalid context area header, write-pos is out if range: %u", writePos);
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    // check parameter
    if (offset == writePos) {
        return DBGUTIL_ERR_END_OF_STREAM;
    }
    if (offset > writePos) {
        LOG_ERROR(sLogger,
                  "Cannot read context record form offset %u: offset exceeds context area size %u",
                  offset, writePos);
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    // read record size and update offset
    recLen = *(uint32_t*)(m_contextArea + offset);
    offset += sizeof(uint32_t);

    // check record is within bounds
    if ((offset + recLen) > writePos) {
        LOG_ERROR(sLogger,
                  "Cannot record record at offset %u, with size %u: record exceeds context area "
                  "valid size %u",
                  offset, recLen, writePos);
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    // set record pointer and update offset
    recPtr = (m_contextArea + offset);
    offset += recLen;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LifeSignManager::readThreadLifeSignDetails(uint32_t threadSlotId, uint64_t& threadId,
                                                      int64_t& startEpochMilliSeconds,
                                                      int64_t& endEpochMilliSeconds,
                                                      bool& isRunning, uint32_t& useCount) {
    // check state
    if (m_lifeSignHeader == nullptr) {
        LOG_ERROR(sLogger,
                  "Cannot read life-sign thread details at slot %u, shared segment not open",
                  threadSlotId);
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // with the slot we can get the thread area's base offset
    uint32_t threadAreaSize = m_lifeSignHeader->m_threadAreaSize;
    LifeSignThreadAreaHeader* hdr =
        (LifeSignThreadAreaHeader*)(m_lifeSignArea + threadSlotId * threadAreaSize);
    threadId = hdr->m_threadId;
    startEpochMilliSeconds = hdr->m_startEpochMilliSeconds;
    endEpochMilliSeconds = hdr->m_endEpochMilliSeconds;
    isRunning = ((hdr->m_state % 2) != 0) ? true : false;
    useCount = (hdr->m_state + 1) / 2;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr LifeSignManager::readLifeSignRecord(uint32_t threadSlotId, uint32_t& offset,
                                               char*& recPtr, uint32_t& recLen,
                                               bool& callerShouldRelease) {
    // the caller should starts with offset zero, and internally it is added to the head offset,
    // until a full circle is reached

    // check state
    if (m_lifeSignHeader == nullptr) {
        LOG_ERROR(sLogger, "Cannot read life-sign record, shared segment not open");
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // with the slot we can get the thread area's base offset
    uint32_t threadAreaSize = m_lifeSignHeader->m_threadAreaSize;
    LifeSignThreadAreaHeader* hdr =
        (LifeSignThreadAreaHeader*)(m_lifeSignArea + threadSlotId * threadAreaSize);
    char* threadArea = (char*)(hdr + 1);

    // get internal state
    if (hdr->m_headPos > DBGUTIL_MAX_LIFE_SIGN_AREA_SIZE_BYTES) {
        LOG_ERROR(sLogger, "Invalid life-sign area header, head-pos is out if range: %u",
                  hdr->m_headPos);
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    if (hdr->m_tailPos > DBGUTIL_MAX_LIFE_SIGN_AREA_SIZE_BYTES) {
        LOG_ERROR(sLogger, "Invalid life-sign area header, tail-pos is out if range: %u",
                  hdr->m_tailPos);
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    // check parameter
    uint32_t cyclicOffset = (hdr->m_headPos + offset) % threadAreaSize;
    if (cyclicOffset >= hdr->m_tailPos) {
        LOG_TRACE(sLogger,
                  "Cannot read life-sign record form offset %u (cyclic-offset %u): offset exceeds "
                  "life-sign area size %u",
                  offset, cyclicOffset, threadAreaSize);
        return DBGUTIL_ERR_END_OF_STREAM;
    }

    // read record size and update offset
    recLen = *(uint32_t*)(threadArea + cyclicOffset);
    uint32_t recLenAligned = ALIGN(recLen, ALIGN_SIZE_BYTES);
    offset += sizeof(uint32_t);
    cyclicOffset = (cyclicOffset + sizeof(uint32_t)) % threadAreaSize;
    uint32_t recEndCyclicOffset = (cyclicOffset + recLenAligned) % threadAreaSize;

    // check thar record does not exceed context area bounds
    if (recEndCyclicOffset > hdr->m_tailPos) {
        LOG_ERROR(sLogger,
                  "Cannot record record at offset %u (cyclic-offset %u), with size %u (cyclic end "
                  "offset %u): record exceeds context area size %u",
                  offset, cyclicOffset, recLen, recEndCyclicOffset, threadAreaSize);
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    // read the record, perhaps while wrapping
    // set record pointer and update offset
    if (cyclicOffset < recEndCyclicOffset) {
        callerShouldRelease = false;
        recPtr = (threadArea + offset);
    } else {
        callerShouldRelease = true;
        recPtr = (char*)malloc(recLen);
        if (recPtr == nullptr) {
            LOG_ERROR(sLogger,
                      "Failed to allocate %u bytes for life-sign record at cyclic offset %u, out "
                      "of memory",
                      recLen, cyclicOffset);
            return DBGUTIL_ERR_NOMEM;
        }
        // copy in parts
        uint32_t bytesTillEnd = threadAreaSize - cyclicOffset;
        uint32_t bytesFromStart = recLen - bytesTillEnd;
        memcpy(recPtr, threadArea + cyclicOffset, bytesTillEnd);
        memcpy(recPtr + bytesTillEnd, threadArea, bytesFromStart);
    }

    // in any case absolute offset should be incremented by aligned length
    offset += recLenAligned;
    return DBGUTIL_ERR_OK;
}

void LifeSignManager::releaseLifeSignRecord(char* recPtr) {
    if (recPtr != nullptr) {
        free(recPtr);
        recPtr = nullptr;
    }
}

DbgUtilErr LifeSignManager::composeShmName(std::string& shmName) {
    std::string processName;
    DbgUtilErr rc = getProcessName(processName);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    std::stringstream s;
    s << DBGUTIL_SHM_PREFIX << "." << processName << "." << getFileTimeStamp() << "."
      << getProcessId() << "." << DBGUTIL_SHM_SUFFIX;
    shmName = s.str();
    return DBGUTIL_ERR_OK;
}

int32_t LifeSignManager::obtainThreadSlot() {
    std::unique_lock<std::mutex> lock(m_lock);
    if (m_vacantSlots.empty()) {
        return -1;
    }

    // find oldest which is always found in the front
    int32_t slot = m_vacantSlots.front();
    setTls(sThreadSlotKey, (void*)(uint64_t)(slot + 1));
    m_vacantSlots.pop_front();
    uint32_t threadAreaSize = m_lifeSignHeader->m_lifeSignAreaSize / m_lifeSignHeader->m_maxThreads;
    LifeSignThreadAreaHeader* hdr =
        (LifeSignThreadAreaHeader*)(m_lifeSignArea + slot * threadAreaSize);
    hdr->m_threadId = getCurrentThreadId();
    hdr->m_headPos = 0;
    hdr->m_tailPos = 0;
    hdr->m_recordCount = 0;
    ++hdr->m_state;  // odd number means running
    hdr->m_startEpochMilliSeconds = getEpochTimeMilliseconds();

    return slot;
}

void LifeSignManager::releaseThreadSlot(int32_t slotId) {
    fprintf(stderr, "Releasing life-sign thread slot %d\n", slotId);
    std::unique_lock<std::mutex> lock(m_lock);
    m_vacantSlots.push_back(slotId);
    uint32_t threadAreaSize = m_lifeSignHeader->m_lifeSignAreaSize / m_lifeSignHeader->m_maxThreads;
    LifeSignThreadAreaHeader* hdr =
        (LifeSignThreadAreaHeader*)(m_lifeSignArea + slotId * threadAreaSize);
    hdr->m_endEpochMilliSeconds = getEpochTimeMilliseconds();
    ++hdr->m_state;  // even number means not running
}

DbgUtilErr initLifeSignManager() {
    registerLogger(sLogger, "life_sign_manager");
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termLifeSignManager() {
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

void setLifeSignManager(LifeSignManager* lifeSignManager) {
    assert((lifeSignManager != nullptr && sLifeSignManager == nullptr) ||
           (lifeSignManager == nullptr && sLifeSignManager != nullptr));
    sLifeSignManager = lifeSignManager;
}

LifeSignManager* getLifeSignManager() {
    assert(sLifeSignManager != nullptr);
    return sLifeSignManager;
}

}  // namespace dbgutil
