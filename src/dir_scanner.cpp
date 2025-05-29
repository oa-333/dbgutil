#include "dir_scanner.h"

#ifndef DBGUTIL_MSVC
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <stdio.h>
#include <string.h>

#include "dbgutil_log_imp.h"

namespace dbgutil {

static Logger sLogger;

void DirScanner::initLogger() { registerLogger(sLogger, "dir_scanner"); }

#ifdef DBGUTIL_MSVC
static DbgUtilErr visitDirEntriesMsvc(const char* dirPath, DirEntryVisitor* visitor) {
    // prepare search pattern
    std::string searchPattern = dirPath;
    searchPattern += "\\*";

    // begin search for files
    WIN32_FIND_DATA findFileData = {};
    HANDLE hFind = FindFirstFile(dirPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_WIN32_ERROR(sLogger, FindFirstFile, "Failed to search for files in directory: %s",
                        dirPath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    // collect all entries
    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ||
            findFileData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL ||
            findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            bool isDir = (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            bool isTrivialDir = false;
            if (isDir) {
                isTrivialDir = strcmp(findFileData.cFileName, ".") == 0 ||
                               strcmp(findFileData.cFileName, "..") == 0;
            }
            if (!isDir || !isTrivialDir) {
                DirEntryInfo entryInfo = {findFileData.cFileName,
                                          isDir ? DirEntryType::DET_DIR : DirEntryType::DET_FILE};
                visitor->onDirEntry(entryInfo);
            }
        }
    } while (FindNextFile(hFind, &findFileData));

    // check for error
    DWORD errCode = GetLastError();
    if (errCode != ERROR_NO_MORE_FILES) {
        LOG_SYS_ERROR_NUM(sLogger, FindNextFile, errCode,
                          "Failed to search for next file in directory: %s", dirPath);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    return DBGUTIL_ERR_OK;
}
#endif

#ifdef DBGUTIL_MINGW
inline DbgUtilErr isRegularFile(const char* path, bool& res) {
    struct stat pathStat;
    if (stat(path, &pathStat) == -1) {
        LOG_SYS_ERROR(sLogger, stat, "Failed to check file %s status", path);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    res = S_ISREG(pathStat.st_mode);
    return DBGUTIL_ERR_OK;
}
#endif

#ifdef DBGUTIL_GCC
static DbgUtilErr visitDirEntriesGcc(const char* dirPath, DirEntryVisitor* visitor) {
    DIR* dirp = opendir(dirPath);
    if (dirp == nullptr) {
        int errCode = errno;
        LOG_SYS_ERROR(sLogger, opendir, "Failed to open directory %s for reading: %d", dirPath,
                      errCode);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }

    struct dirent* dir = nullptr;
#ifdef DBGUTIL_MINGW
    std::string basePath = dirPath;
    while ((dir = readdir(dirp)) != nullptr) {
        bool isRegular = false;
        DbgUtilErr rc = isRegularFile((basePath + "/" + dir->d_name).c_str(), isRegular);
        if (rc != DBGUTIL_ERR_OK) {
            closedir(dirp);
            return rc;
        }
        bool isTrivialDir = false;
        if (!isRegular) {
            isTrivialDir = strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0;
        }
        if (isRegular || !isTrivialDir) {
            DirEntryInfo entryInfo = {dir->d_name,
                                      isRegular ? DirEntryType::DET_FILE : DirEntryType::DET_DIR};
            visitor->onDirEntry(entryInfo);
        }
    }
#else
    // man page says that errno should be set to zero before calling readdir, in order to be able to
    // distinguished between end of stream and real error
    errno = 0;
    while ((dir = readdir(dirp)) != nullptr) {
        bool isRegularFile = (dir->d_type == DT_REG);
        bool isTrivialDir = false;
        if (!isRegularFile) {
            isTrivialDir = strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0;
        }
        if (isRegularFile || !isTrivialDir) {
            DirEntryInfo entryInfo = {
                dir->d_name, isRegularFile ? DirEntryType::DET_FILE : DirEntryType::DET_DIR};
            visitor->onDirEntry(entryInfo);
        }
    }
#endif
    int errCode = errno;
    if (errCode != 0) {
        LOG_SYS_ERROR(sLogger, readdir, "Failed to list files in directory %s: %d", dirPath,
                      errCode);
        closedir(dirp);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    if (closedir(dirp) < 0) {
        LOG_SYS_ERROR(sLogger, closedir, "Failed to terminate listing files in directory %s: %d",
                      dirPath, errCode);
        return DBGUTIL_ERR_SYSTEM_FAILURE;
    }
    return DBGUTIL_ERR_OK;
}
#endif

DbgUtilErr DirScanner::visitDirEntries(const char* dirPath, DirEntryVisitor* visitor) {
#ifdef DBGUTIL_MSVC
    return visitDirEntriesMsvc(dirPath, visitor);
#else
    return visitDirEntriesGcc(dirPath, visitor);
#endif
}

DbgUtilErr DirScanner::scanDirEntries(const char* dirPath, std::vector<DirEntryInfo>& dirEntries) {
    class DirEntryCollector : public DirEntryVisitor {
    public:
        DirEntryCollector(std::vector<DirEntryInfo>& dirEntries) : m_dirEntries(dirEntries) {}

        void onDirEntry(const DirEntryInfo& dirEntry) final { m_dirEntries.push_back(dirEntry); }

    private:
        std::vector<DirEntryInfo>& m_dirEntries;
    };
    DirEntryCollector dirEntryCollector(dirEntries);
    return visitDirEntries(dirPath, &dirEntryCollector);
}

DbgUtilErr DirScanner::scanDirFiles(const char* dirPath, std::vector<std::string>& fileNames) {
    class FileCollector : public DirEntryVisitor {
    public:
        FileCollector(std::vector<std::string>& fileNames) : m_fileNames(fileNames) {}

        void onDirEntry(const DirEntryInfo& dirEntry) final {
            if (dirEntry.m_type == DirEntryType::DET_FILE) {
                m_fileNames.push_back(dirEntry.m_name);
            }
        }

    private:
        std::vector<std::string>& m_fileNames;
    };
    FileCollector fileCollector(fileNames);
    return visitDirEntries(dirPath, &fileCollector);
}

DbgUtilErr DirScanner::scanDirDirs(const char* dirPath, std::vector<std::string>& dirNames) {
    class DirCollector : public DirEntryVisitor {
    public:
        DirCollector(std::vector<std::string>& dirNames) : m_dirNames(dirNames) {}

        void onDirEntry(const DirEntryInfo& dirEntry) final {
            if (dirEntry.m_type == DirEntryType::DET_DIR) {
                m_dirNames.push_back(dirEntry.m_name);
            }
        }

    private:
        std::vector<std::string>& m_dirNames;
    };
    DirCollector dirCollector(dirNames);
    return visitDirEntries(dirPath, &dirCollector);
}

}  // namespace dbgutil