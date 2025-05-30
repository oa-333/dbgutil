#ifndef __PATH_PARSER_H__
#define __PATH_PARSER_H__

#include <string>
#include <vector>

#include "dbgutil_common.h"

namespace dbgutil {

/** @brief A syntactical path parsing utility class. */
class DBGUTIL_API PathParser {
public:
    static void initLogger();
    static void termLogger();

    /**
     * @brief Canonicalizes a path.
     * @param path The file or directory path.
     * @param[out] canonPath The resulting canonical path.
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If the provided path is illegal.
     */
    static DbgUtilErr canonicalizePath(const char* path, std::string& canonPath);

    /**
     * @brief Canonicalizes a path, but instead return the canonical path components.
     * @param path The file or directory path.
     * @param[out] components The resulting canonical path components.
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If the provided path is illegal.
     */
    static DbgUtilErr canonicalizePathComponents(const char* path,
                                                 std::vector<std::string>& components);

    /**
     * @brief Queries whether a path is syntactically legal. This includes both path syntax check
     * and file name check.
     * @param path The file or directory path to check.
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If the provided path is illegal.
     */
    static DbgUtilErr isPathLegal(const char* path);

    /**
     * @brief Queries whether a path is an absolute path, starting from root. No path syntax checks
     * are made.
     * @param path The file or directory path to check.
     * @return true If the path is absolute.
     * @return false If the path is not absolute.
     */
    static bool isPathAbsolute(const char* path);

    /**
     * @brief Queries whether a path component list is legal. This basically check for illegal
     * character in each path component.
     * @param components The path component list to check.
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If any component in the provided list is illegal.
     */
    static DbgUtilErr isPathComponentListLegal(const std::vector<std::string>& components);

    /**
     * @brief Queries whether a single path component is legal. This basically check for illegal
     * character in the path component.
     * @param pathComponent The path component to check.
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If the provided path component is illegal.
     */
    static DbgUtilErr isPathComponentLegal(const char* pathComponent);

    /**
     * @brief Parse a path into its components.
     * @param path The file or directory path.
     * @param[out] components The path components.
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If the provided path is illegal.
     */
    static DbgUtilErr parsePath(const char* path, std::vector<std::string>& components);

    /**
     * @brief Retrieves the parent path of a given path. In essence retrieves the parent directory.
     * @param path The file or directory path.
     * @param[out] parentPath The resulting parent path.
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If the provided path is illegal.
     */
    static DbgUtilErr getParentPath(const char* path, std::string& parentPath);

    /**
     * @brief Extracts the bare file name from a path.
     * @param path The file or directory path.
     * @param[out] fileName The bare file name (stripped from its parent directory path).
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If the provided path is illegal.
     */
    static DbgUtilErr getFileName(const char* path, std::string& fileName);

    /**
     * @brief Composes a path from two parts components with syntax check.
     * @param basePath The base path.
     * @param subPath The sub-path.
     * @param[out] path The resulting path.
     * @return E_OK If the operation succeeded.
     * @return E_INVALID_ARGUMENT If the provided path is illegal.
     */
    static DbgUtilErr composePath(const char* basePath, const char* subPath, std::string& path);

    /**
     * @brief Composes a path from separate components.
     * @param components The path components.
     * @param[out] path The resulting path.
     */
    static void composePath(const std::vector<std::string>& components, std::string& path);

    /**
     * @brief Composes a path from components range.
     * @param from The range start.
     * @param to The range end (not including).
     * @param[out] path The resulting path.
     */
    static void composePath(std::vector<std::string>::const_iterator from,
                            std::vector<std::string>::const_iterator to, std::string& path);
};

}  // namespace dbgutil

#endif  // __PATH_PARSER_H__