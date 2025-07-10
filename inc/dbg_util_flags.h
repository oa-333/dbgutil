#ifndef __DBG_UTIL_FLAGS_H__
#define __DBG_UTIL_FLAGS_H__

/** @brief Specifies whether dbgutil should register signal/exception handlers. */
#define DBGUTIL_CATCH_EXCEPTIONS 0x0001

/** @brief Specifies whether dbgutil should register terminate() handler. */
#define DBGUTIL_SET_TERMINATE_HANDLER 0x0002

/** @brief Specifies whether dbgutil should write to log any caught exception.  */
#define DBGUTIL_LOG_EXCEPTIONS 0x0004

/** @brief Specifies whether dbgutil should attempt to write a mini-dump core file when crashing. */
#define DBGUTIL_WIN32_MINI_DUMP_CORE 0x0008

/** @brief Turns on all flags/options. */
#define DBGUTIL_FLAGS_ALL 0xFFFFFFFF

#endif  // __DBG_UTIL_DEF_H__