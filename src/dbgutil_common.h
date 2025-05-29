#ifndef __DBGUTIL_COMMON_H__
#define __DBGUTIL_COMMON_H__

#include <cstdint>
#include <string>

#include "dbg_util_def.h"
#include "dbg_util_err.h"
#include "dbg_util_log.h"

namespace dbgutil {

/**
 * @brief Converts error code to string.
 * @note This mechanism is made extensible to other libraries via @ref registerErrorCodeHandler().
 * @param errorCode The error code to convert.
 * @return const char* The resulting error string. This will never be null.
 */
extern const char* errorCodeToStr(DbgUtilErr rc);

/** @brief Trims a string's prefix from the left side (in-place). */
inline void ltrim(std::string& s) { s.erase(0, s.find_first_not_of(" \n\r\t")); }

/** @brief Trims a string suffix from the right side (in-place). */
inline void rtrim(std::string& s) { s.erase(s.find_last_not_of(" \n\r\t") + 1); }

/** @brief Trims a string from both sides (in-place). */
inline std::string trim(const std::string& s) {
    std::string res = s;
    ltrim(res);
    rtrim(res);
    return res;
}

// TODO: reorganize this file

}  // namespace dbgutil

#endif  // __DBGUTIL_COMMON_H__