#ifndef __DBG_UTIL_DEF_H__
#define __DBG_UTIL_DEF_H__

// Windows/MSVC
#ifdef _MSC_VER
// include windows header unless instructed not to do so
#ifndef DBGUTIL_NO_WINDOWS_HEADER
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#define DBGUTIL_WINDOWS
#define DBGUTIL_MSVC

#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)
#ifdef DBGUTIL_DLL
#define DBGUTIL_API DLL_EXPORT
#else
#define DBGUTIL_API DLL_IMPORT
#endif

// Windows/MinGW stuff
#elif defined(__MINGW32__) || defined(__MINGW64__)
#define DBGUTIL_WINDOWS
#define DBGUTIL_MINGW
#define DBGUTIL_GCC
#define DBGUTIL_API

// Linux stuff
#elif defined(__linux__)
#define DBGUTIL_LINUX
#define DBGUTIL_GCC
#define DBGUTIL_API

// unsupported platform
#else
#error "Unsupported platform"
#endif

// define strcasecmp for MSVC
#ifdef DBGUTIL_MSVC
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

namespace libdbg {

/** @typedef Platform-independent thread id type. */
#ifdef DBGUTIL_WINDOWS
typedef unsigned long os_thread_id_t;
#define PRItid "lu"
#else
typedef long os_thread_id_t;
#define PRItid "ld"
#endif
#define PRItidx "lx"

/** @typedef Platform-independent UTM time type. */
typedef unsigned long long app_time_t;

/** @brief Retrieves current thread identifier. */
extern DBGUTIL_API os_thread_id_t getCurrentThreadId();

}  // namespace libdbg

#endif  // __DBG_UTIL_DEF_H__