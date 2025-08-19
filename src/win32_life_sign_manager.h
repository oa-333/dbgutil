#ifndef __WIN32_LIFE_SIGN_MANAGER_H__
#define __WIN32_LIFE_SIGN_MANAGER_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS

#include "life_sign_manager.h"
#include "win32_shm.h"

namespace dbgutil {

class Win32LifeSignManager : public LifeSignManager {
public:
    /** @brief Creates the singleton instance of the module manager for Windows platform. */
    static DbgUtilErr createInstance();

    /** @brief Retrieves a reference to the single instance of the module manager. */
    static Win32LifeSignManager* getInstance();

    /** @brief Destroys the singleton instance of the module manager. */
    static void destroyInstance();

    /** @brief Destroys a previously created life-sign shared memory segment. */
    DbgUtilErr deleteLifeSignShmSegment(const char* segmentName) final;

protected:
    /** @brief Creates a shared memory segment object. */
    OsShm* createShmObject() final;

    /** @brief Destroys the shared memory segment object. */
    void destroyShmObject() final;

    DbgUtilErr getImagePath(std::string& imagePath) final;
    DbgUtilErr getProcessName(std::string& processName) final;
    uint32_t getProcessId() final;
    std::string getFileTimeStamp() final;
    const char* getShmPath() final;
    DbgUtilErr getShmFileSize(const char* shmFilePath, uint32_t& shmSize) final;

private:
    Win32LifeSignManager() : m_win32Shm(nullptr) {}
    Win32LifeSignManager(const Win32LifeSignManager&) = delete;
    Win32LifeSignManager(Win32LifeSignManager&&) = delete;
    Win32LifeSignManager& operator=(const Win32LifeSignManager&) = delete;
    ~Win32LifeSignManager() final;

    static Win32LifeSignManager* sInstance;

    Win32Shm* m_win32Shm;
};

extern DbgUtilErr initWin32LifeSignManager();
extern DbgUtilErr termWin32LifeSignManager();

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS

#endif  // __WIN32_LIFE_SIGN_MANAGER_H__