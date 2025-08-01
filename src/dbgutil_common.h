#ifndef __DBGUTIL_COMMON_H__
#define __DBGUTIL_COMMON_H__

#include <cstdint>
#include <string>

#include "dbg_util_def.h"
#include "libdbg_err.h"
#include "libdbg_log.h"

namespace libdbg {

/**
 * @brief Converts error code to string.
 * @note This mechanism is made extensible to other libraries via @ref registerErrorCodeHandler().
 * @param errorCode The error code to convert.
 * @return const char* The resulting error string. This will never be null.
 */
extern const char* errorCodeToStr(LibDbgErr rc);

/** @brief Sets the global flags settings for dbgutil. */
extern void setGlobalFlags(uint32_t flags);

/** @brief Sets the global flags settings for dbgutil. */
extern uint32_t getGlobalFlags();

/**
 * @brief Safer and possibly/hopefully faster version of strncpy() (not benchmarked yet). Unlike
 * strncpy(), this implementation has three notable differences:
 * (1) The resulting destination always has a terminating null
 * (2) In case of a short source string, the resulting destination is not padded with many nulls up
 * to the size limit, but rather only one terminating null is added
 * (3) The result value is the number of characters copied, not including the terminating null.
 * @param dest The destination string.
 * @param src The source string.
 * @param destLen The destination length.
 * @param srcLen The source length (optional, can run faster if provided).
 * @return The number of characters copied.
 */
extern size_t dbgutil_strncpy(char* dest, const char* src, size_t destLen, size_t srcLen = 0);

}  // namespace libdbg

#endif  // __DBGUTIL_COMMON_H__