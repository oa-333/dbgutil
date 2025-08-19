#include <cstdlib>

#include "dbg_util_err.h"

namespace dbgutil {

static const char* sErrorStrings[] = {
    "No error",          // DBGUTIL_ERR_OK
    "Out of memory",     // DBGUTIL_ERR_NOMEM
    "Invalid argument",  // DBGUTIL_ERR_INVALID_ARGUMENT
    "Invalid state",     // DBGUTIL_ERR_INVALID_STATE
    "Resource limit",    // DBGUTIL_ERR_RESOURCE_LIMIT
    "system failure",    // DBGUTIL_ERR_SYSTEM_FAILURE
    "not found",         // DBGUTIL_ERR_NOT_FOUND
    "Internal error",    // DBGUTIL_ERR_INTERNAL_ERROR
    "End of file",       // DBGUTIL_ERR_EOF
    "Already exists",    // DBGUTIL_ERR_ALREADY_EXISTS
    "Access denied",     // DBGUTIL_ERR_ACCESS_DENIED
    "End of stream",     // DBGUTIL_ERR_END_OF_STREAM
    "Not implemented",   // DBGUTIL_ERR_NOT_IMPLEMENTED
    "Data corrupt",      // DBGUTIL_ERR_DATA_CORRUPT
    "Resource is busy"   // DBGUTIL_ERR_RESOURCE_BUSY
};

static const size_t sErrorCount = sizeof(sErrorStrings) / sizeof(sErrorStrings[0]);

const char* errorToString(DbgUtilErr err) {
    if (err < sErrorCount) {
        return sErrorStrings[err];
    }
    return "N/A";
}

}  // namespace dbgutil