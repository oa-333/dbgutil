#include "path_parser.h"

#include <cassert>
#include <cstring>
#include <list>

#include "dbgutil_log_imp.h"
#include "os_util.h"

#ifdef DBGUTIL_WINDOWS
#define DBGUTIL_PATH_SEP '\\'
#define DBGUTIL_PATH_SEP_CHARS "\\/"
#else
#define DBGUTIL_PATH_SEP '/'
#define DBGUTIL_PATH_SEP_CHARS "/"
#endif

namespace dbgutil {

static Logger sLogger;

void PathParser::initLogger() { registerLogger(sLogger, "path_parser"); }
void PathParser::termLogger() { unregisterLogger(sLogger); }

DbgUtilErr PathParser::canonicalizePath(const char* path, std::vector<std::string>& components) {
    std::vector<std::string> pathComps;
    DbgUtilErr rc = parsePath(path, pathComps);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // process components: discard '.', pop for '..', otherwise push
    for (const std::string& comp : components) {
        if (comp.compare(".") == 0) {
            // discard, unless this is the first component, which is replaced by current directory
            if (components.empty()) {
                std::string currentDir;
                rc = OsUtil::getCurrentDir(currentDir);
                if (rc != DBGUTIL_ERR_OK) {
                    return rc;
                }
                std::vector<std::string> dirComponents;
                rc = parsePath(currentDir.c_str(), dirComponents);
                if (rc != DBGUTIL_ERR_OK) {
                    return rc;
                }
                components.insert(components.end(), dirComponents.begin(), dirComponents.end());
            }
        } else if (comp.compare("..") == 0) {
            if (components.empty()) {
                LOG_ERROR(sLogger, "Cannot canonicalize path, invalid path specification: %s",
                          path);
                return DBGUTIL_ERR_INVALID_ARGUMENT;
            } else {
                components.pop_back();
            }
        } else {
            components.push_back(comp);
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::canonicalizePath(const char* path, std::string& canonPath) {
    // break path into canonicalized components
    std::vector<std::string> components;
    DbgUtilErr rc = canonicalizePath(path, components);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // compose back path from components
    composePath(components, canonPath);
    LOG_TRACE(sLogger, "Formed canonical path: %s --> %s", path, canonPath.c_str());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::normalizePath(const char* path, std::string& canonPath) {
    std::vector<std::string> components;
    DbgUtilErr rc = normalizePath(path, components);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // compose back path from components
    composePath(components, canonPath);
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::normalizePath(const char* path, std::vector<std::string>& components) {
    // process components: discard '.', pop for '..', otherwise push
    std::vector<std::string> compStack;
    for (const std::string& comp : components) {
        if (comp.compare(".") == 0) {
            // discard
        } else if (comp.compare("..") == 0) {
            if (compStack.empty()) {
                LOG_ERROR(sLogger, "Cannot canonicalize path, invalid path specification: %s",
                          path);
                return DBGUTIL_ERR_INVALID_ARGUMENT;
            } else {
                compStack.pop_back();
            }
        } else {
            compStack.push_back(comp);
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::isPathLegal(const char* path) {
    // break path into canonicalized components
    std::vector<std::string> components;
    DbgUtilErr rc = canonicalizePath(path, components);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // now check each component
    return isPathComponentListLegal(components);
}

bool PathParser::isPathAbsolute(const char* path) {
    if (path == nullptr) {
        return false;
    }
#ifdef DBGUTIL_WINDOWS
    if (strchr(DBGUTIL_PATH_SEP_CHARS, path[0]) != nullptr) {
        return true;
    }
    // check also for path starting with <DriveLetter>:
    if (path[0] != 0 && path[1] == ':') {
        return true;
    }
    return false;
#else
    return strchr(DBGUTIL_PATH_SEP_CHARS, path[0]) != nullptr;
#endif
}

DbgUtilErr PathParser::isPathComponentListLegal(const std::vector<std::string>& components) {
    for (const std::string& comp : components) {
        DbgUtilErr rc = isPathComponentLegal(comp.c_str());
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::isPathComponentLegal(const char* pathComponent) {
    // basically only normal ascii chars are allowed: a-z, A-Z, 0-9, _, -
    // these are strictly forbidden: <>:;"'|\/?@#$%^&*()=+`
    std::string pathCompStr = pathComponent;
    std::string illegalChars = "<>:;\"'|\\/?@#$%^&*()=+`";
    if (pathCompStr.find_first_of(illegalChars) != std::string::npos) {
        LOG_ERROR(sLogger, "Invalid path component: %s", pathComponent);
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::parsePath(const char* path, std::vector<std::string>& components) {
    // we parse a path by separator char
    // note: we allow mixing forward anf backward slash
    std::string pathStr = path;
    std::string::size_type prevPos = 0;
    std::string::size_type pos = pathStr.find_first_of(DBGUTIL_PATH_SEP_CHARS);
    while (pos != std::string::npos) {
        if (pos > prevPos) {
            components.push_back(pathStr.substr(prevPos, pos - prevPos));
        }
        prevPos = pathStr.find_first_not_of(DBGUTIL_PATH_SEP_CHARS, pos);
        if (prevPos != std::string::npos) {
            pos = pathStr.find_first_of(DBGUTIL_PATH_SEP_CHARS, prevPos);
        }
    }
    components.push_back(pathStr.substr(prevPos));
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::getParentPath(const char* path, std::string& parentPath) {
    // break path into components
    std::vector<std::string> components;
    DbgUtilErr rc = parsePath(path, components);
    if (rc == DBGUTIL_ERR_OK) {
        // note: the parent of root directory is the root directory
        if (components.size() > 0) {
            components.resize(components.size() - 1);
        }
        composePath(components, parentPath);
    }
    return rc;
}

DbgUtilErr PathParser::getFileName(const char* path, std::string& fileName) {
    // break path into components
    std::vector<std::string> components;
    DbgUtilErr rc = parsePath(path, components);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    if (components.size() == 0) {
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }
    fileName = components[components.size() - 1];
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::composePath(const char* basePath, const char* subPath, std::string& path,
                                   bool canonicalize /* = true */) {
    if (!canonicalize) {
        path = std::string(basePath) + DBGUTIL_PATH_SEP + subPath;
        // TODO: MinGW can have mixed forward and backward slash, so it need more normalizing
        return DBGUTIL_ERR_OK;
    }

    if (isPathAbsolute(subPath)) {
        LOG_ERROR(sLogger, "Cannot compose base path '%s' with sub-path '%s': sub-path is absolute",
                  basePath, subPath);
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    std::string canonBasePath;
    DbgUtilErr rc = canonicalizePath(basePath, canonBasePath);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    std::string tmpPath = canonBasePath + DBGUTIL_PATH_SEP + subPath;
    // TODO: MinGW can have mixed forward and backward slash, so it need more normalizing
    return canonicalizePath(tmpPath.c_str(), path);
}

void PathParser::composePath(const std::vector<std::string>& components, std::string& path) {
    // NOTE: compose path (iterator variant) is naive and gives expected result only on Unix (it
    // yields an absolute path starting with slash), so we have to be careful on Windows/MinGW,
    // since a path can start with drive letter
#ifdef DBGUTIL_WINDOWS
    if (components.front().length() > 1 && components.front()[1] == ':') {
        std::string tmpPath;
        composePath(components.begin() + 1, components.end(), tmpPath);
        // NOTE: the composes tmp path has a leading slash, so we just need to concatenate
        assert(tmpPath[0] == DBGUTIL_PATH_SEP);
        path = components.front() + tmpPath;
        LOG_TRACE(sLogger, "Composed path: %s", path.c_str());
        return;
    }
#endif

    composePath(components.begin(), components.end(), path);
}

void PathParser::composePath(std::vector<std::string>::const_iterator from,
                             std::vector<std::string>::const_iterator to, std::string& path) {
    // TODO: On MinGW it is possible we have a C:\ path-style, in which case forward slash should be
    // used
    path.clear();
    while (from != to) {
        path += DBGUTIL_PATH_SEP;
        path += *from;
        ++from;
    }
    if (path.empty()) {
        path += DBGUTIL_PATH_SEP;
    }
}

}  // namespace dbgutil
