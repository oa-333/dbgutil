#ifndef __DBGUTIL_WIN32_DLL_EVENT_H__
#define __DBGUTIL_WIN32_DLL_EVENT_H__

#include "libdbg_def.h"

#ifdef DBGUTIL_WINDOWS

#include <cstdint>

namespace libdbg {

#define DBGUTIL_DLL_PROCESS_ATTACH 1
#define DBGUTIL_DLL_PROCESS_DETACH 2
#define DBGUTIL_DLL_THREAD_ATTACH 3
#define DBGUTIL_DLL_THREAD_DETACH 4

typedef void (*ThreadDllEventCB)(int, void*);

class DllListener {
public:
    virtual ~DllListener() {}

    virtual void onThreadDllAttach() = 0;
    virtual void onThreadDllDetach() = 0;
    virtual void onProcessDllDetach() = 0;

protected:
    DllListener() {}
    DllListener(const DllListener&) = delete;
    DllListener(DllListener&&) = delete;
    DllListener& operator=(const DllListener&) = delete;
};

class DllPurgeFilter {
public:
    virtual ~DllPurgeFilter() {}

    virtual bool purge(ThreadDllEventCB callback, void* userData) = 0;

protected:
    DllPurgeFilter() {}
    DllPurgeFilter(const DllPurgeFilter&) = delete;
    DllPurgeFilter(DllPurgeFilter&&) = delete;
    DllPurgeFilter& operator=(const DllPurgeFilter&) = delete;
};

// listener API
extern void registerDllListener(DllListener* listener);
extern void deregisterDllListener(DllListener* listener);

// callback API
extern void registerDllCallback(ThreadDllEventCB callback, void* userData);
extern void deregisterDllCallback(ThreadDllEventCB callback);
extern void* getDllCallbackUserData(ThreadDllEventCB callback);
extern void purgeDllCallback(DllPurgeFilter* filter);

}  // namespace libdbg

#endif  // DBGUTIL_WINDOWS

#endif  // __DBGUTIL_WIN32_DLL_EVENT_H__