#ifndef __LINUX_LIFE_SIGN_MANAGER_H__
#define __LINUX_LIFE_SIGN_MANAGER_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_LINUX

#include "life_sign_manager.h"
#include "linux_shm.h"

namespace dbgutil {

class LinuxLifeSignManager : public LifeSignManager {
public:
    /** @brief Creates the singleton instance of the module manager for Windows platform. */
    static DbgUtilErr createInstance();

    /** @brief Retrieves a reference to the single instance of the module manager. */
    static LinuxLifeSignManager* getInstance();

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
    LinuxLifeSignManager() : m_linuxShm(nullptr) {}
    LinuxLifeSignManager(const LinuxLifeSignManager&) = delete;
    LinuxLifeSignManager(LinuxLifeSignManager&&) = delete;
    LinuxLifeSignManager& operator=(const LinuxLifeSignManager&) = delete;
    ~LinuxLifeSignManager() final;

    static LinuxLifeSignManager* sInstance;

    LinuxShm* m_linuxShm;
};

extern DbgUtilErr initLinuxLifeSignManager();
extern DbgUtilErr termLinuxLifeSignManager();

}  // namespace dbgutil

#endif  // DBGUTIL_LINUX

#endif  // __LINUX_LIFE_SIGN_MANAGER_H__