#include "path_parser.h"

#include <list>

#include "dbgutil_log_imp.h"

// support only unix path's at the moment
#define CSI_PATH_SEP '/'

namespace dbgutil {

static Logger sLogger;

void PathParser::initLogger() { registerLogger(sLogger, "path_parser"); }
void PathParser::termLogger() { unregisterLogger(sLogger); }

DbgUtilErr PathParser::canonicalizePathComponents(const char* path,
                                                  std::vector<std::string>& components) {
    DbgUtilErr rc = parsePath(path, components);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

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

DbgUtilErr PathParser::canonicalizePath(const char* path, std::string& canonPath) {
    // break path into canonicalized components
    std::vector<std::string> components;
    DbgUtilErr rc = canonicalizePathComponents(path, components);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // compose back path from components
    composePath(components, canonPath);
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::isPathLegal(const char* path) {
    // break path into canonicalized components
    std::vector<std::string> components;
    DbgUtilErr rc = canonicalizePathComponents(path, components);
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
    return (path[0] == CSI_PATH_SEP);
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
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr PathParser::parsePath(const char* path, std::vector<std::string>& components) {
    // we parse a path by separator char
    // note: we allow mixing forward anf backward slash
    std::string sep = "\\/";
    std::string pathStr = path;
    std::string::size_type prevPos = 0;
    std::string::size_type pos = pathStr.find_first_of(sep);
    while (pos != std::string::npos) {
        if (pos > prevPos) {
            components.push_back(pathStr.substr(prevPos, pos - prevPos));
        }
        prevPos = pathStr.find_first_not_of(sep, pos);
        if (prevPos != std::string::npos) {
            pos = pathStr.find_first_of(sep, prevPos);
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

DbgUtilErr PathParser::composePath(const char* basePath, const char* subPath, std::string& path) {
    if (isPathAbsolute(subPath)) {
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    std::string canonBasePath;
    DbgUtilErr rc = canonicalizePath(basePath, canonBasePath);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    std::string tmpPath = canonBasePath + CSI_PATH_SEP + subPath;
    return canonicalizePath(tmpPath.c_str(), path);
}

void PathParser::composePath(const std::vector<std::string>& components, std::string& path) {
    composePath(components.begin(), components.end(), path);
}

void PathParser::composePath(std::vector<std::string>::const_iterator from,
                             std::vector<std::string>::const_iterator to, std::string& path) {
    path.clear();
    while (from != to) {
        path += CSI_PATH_SEP;
        path += *from;
        ++from;
    }
    if (path.empty()) {
        path += CSI_PATH_SEP;
    }
}

}  // namespace dbgutil
