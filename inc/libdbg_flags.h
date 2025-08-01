#ifndef __LIBDBG_FLAGS_H__
#define __LIBDBG_FLAGS_H__

/** @brief Specifies whether dbgutil should register signal/exception handlers. */
#define LIBDBG_CATCH_EXCEPTIONS 0x0001

/** @brief Specifies whether dbgutil should register terminate() handler. */
#define LIBDBG_SET_TERMINATE_HANDLER 0x0002

/** @brief Specifies whether dbgutil should write to log any caught exception.  */
#define LIBDBG_LOG_EXCEPTIONS 0x0004

/** @brief Specifies whether dbgutil should dump core file when crashing. */
#define LIBDBG_EXCEPTION_DUMP_CORE 0x0008

/** @brief Turns on all flags/options. */
#define LIBDBG_FLAGS_ALL 0xFFFFFFFF

#endif  // __LIBDBG_FLAGS_H__