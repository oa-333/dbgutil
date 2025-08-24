#include "dbgutil_common.h"

#include <cassert>
#include <cstring>

namespace dbgutil {

static uint32_t sFlags = 0;

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
