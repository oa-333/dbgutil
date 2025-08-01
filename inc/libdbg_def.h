#ifndef __LIBDBG_DEF_H__
#define __LIBDBG_DEF_H__

// Windows/MSVC
#ifdef _MSC_VER
// include windows header unless instructed not to do so
#ifndef LIBDBG_NO_WINDOWS_HEADER
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#define LIBDBG_WINDOWS
#define LIBDBG_MSVC

#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllimport)
#ifdef LIBDBG_DLL
#define LIBDBG_API DLL_EXPORT
#else
#define LIBDBG_API DLL_IMPORT
#endif

// Windows/MinGW stuff
#elif defined(__MINGW32__) || defined(__MINGW64__)
#define LIBDBG_WINDOWS
#define LIBDBG_MINGW
#define LIBDBG_GCC
#define LIBDBG_API

// Linux stuff
#elif defined(__linux__)
#define LIBDBG_LINUX
#define LIBDBG_GCC
#define LIBDBG_API

// unsupported platform
#else
#error "Unsupported platform"
#endif

// define strcasecmp for MSVC
#ifdef LIBDBG_MSVC
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

namespace libdbg {

/** @typedef Platform-independent thread id type. */
#ifdef LIBDBG_WINDOWS
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
extern LIBDBG_API os_thread_id_t getCurrentThreadId();

}  // namespace libdbg

#endif  // __LIBDBG_DEF_H__