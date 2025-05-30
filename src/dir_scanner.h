#ifndef __DIR_SCANNER_H__
#define __DIR_SCANNER_H__

#include <string>
#include <vector>

#include "dbgutil_common.h"

namespace dbgutil {

/** @enum Directory entry type constants. */
enum class DirEntryType {
    /** @var Entry type is a regular file. */
    DET_FILE,

    /** @var Entry type is a directory. */
    DET_DIR
};

/** @struct Single directory entry information. */
struct DBGUTIL_API DirEntryInfo {
    /** @brief Bare entry name (no containing directory prefix). */
    std::string m_name;

    /** @brief The directory entry type. */
    DirEntryType m_type;
};

class DBGUTIL_API DirEntryVisitor {
public:
    virtual ~DirEntryVisitor() {}

    virtual void onDirEntry(const DirEntryInfo& dirEntry) = 0;

protected:
    DirEntryVisitor() {}
};

/** @class Utility class for scanning directory files. */
class DBGUTIL_API DirScanner {
public:
    static void initLogger();
    static void termLogger();

    /**
     * @brief Scans for directory entries (files or directories).
     * @param dirPath The directory path to scan.
     * @param[out] dirEntries The resulting directory entries.
     * @return DbgUtilErr Operation result.
     */
    static DbgUtilErr visitDirEntries(const char* dirPath, DirEntryVisitor* visitor);

    /**
     * @brief Scans for directory entries (files or directories).
     * @param dirPath The directory path to scan.
     * @param[out] dirEntries The resulting directory entries.
     * @return DbgUtilErr Operation result.
     */
    static DbgUtilErr scanDirEntries(const char* dirPath, std::vector<DirEntryInfo>& dirEntries);

    /**
     * @brief Scans for directory files (regular only).
     * @param dirPath The directory path to scan.
     * @param[out] fileNames The resulting file names (no directory prepended).
     * @return DbgUtilErr Operation result.
     */
    static DbgUtilErr scanDirFiles(const char* dirPath, std::vector<std::string>& fileNames);

    /**
     * @brief Scans for directory's sub-directories.
     * @param dirPath The directory path to scan.
     * @param[out] dirNames The resulting directory names (no parent directory prepended).
     * @return DbgUtilErr Operation result.
     */
    static DbgUtilErr scanDirDirs(const char* dirPath, std::vector<std::string>& dirNames);

private:
    DirScanner() {}
    DirScanner(const DirScanner&) = delete;
    ~DirScanner() {}
};

}  // namespace dbgutil

#endif  // __DIR_SCANNER_H__