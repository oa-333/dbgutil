#include "dbgutil_common.h"

#include <cassert>
#include <cstring>

namespace dbgutil {

static uint32_t sFlags = 0;

static const char* sErrorCodeStr[] = {
    "No error",          // DBGUTIL_ERR_OK
    "Out of memory",     // DBGUTIL_ERR_NOMEM
    "Invalid argument",  // DBGUTIL_ERR_INVALID_ARGUMENT
    "Invalid state",     // DBGUTIL_ERR_INVALID_STATE
    "Resource limit",    // DBGUTIL_ERR_RESOURCE_LIMIT
    "System failure",    // DBGUTIL_ERR_SYSTEM_FAILURE
    "Not found",         // DBGUTIL_ERR_NOT_FOUND
    "Internal error",    // DBGUTIL_ERR_INTERNAL_ERROR
    "End of file",       // DBGUTIL_ERR_EOF
    "Already exists",    // DBGUTIL_ERR_ALREADY_EXISTS
    "Access denied",     // DBGUTIL_ERR_ACCESS_DENIED
    "End of stream",     // DBGUTIL_ERR_END_OF_STREAM
    "Not implemented",   // DBGUTIL_ERR_NOT_IMPLEMENTED
    "Data corrupted",    // DBGUTIL_ERR_DATA_CORRUPT
    "Resource busy",     // DBGUTIL_ERR_RESOURCE_BUSY
};

const char* errorCodeToStr(DbgUtilErr rc) {
    if (rc <= DBGUTIL_ERR_RESOURCE_BUSY) {
        return sErrorCodeStr[(uint32_t)rc];
    }
    return "Unknown error";
}

void setGlobalFlags(uint32_t flags) { sFlags = flags; }

uint32_t getGlobalFlags() { return sFlags; }

size_t dbgutil_strncpy(char* dest, const char* src, size_t destLen, size_t srcLen /* = 0 */) {
    assert(destLen > 0);
    if (srcLen == 0) {
        srcLen = strlen(src);
    }
    if (srcLen + 1 < destLen) {
        // copy terminating null as well (use faster memcpy())
        memcpy(dest, src, srcLen + 1);
        return srcLen;
    }
    // reserve one char for terminating null
    size_t copyLen = destLen - 1;
    memcpy(dest, src, copyLen);

    // add terminating null
    dest[copyLen] = 0;

    // return number of bytes copied, excluding terminating null
    return copyLen;
}

}  // namespace dbgutil
