#include "dbgutil_tls.h"

#include "dbg_util_err.h"
#include "dbgutil_log_imp.h"

#ifdef DBGUTIL_MINGW
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#ifdef DBGUTIL_WINDOWS
#include <cstdio>
#include <new>

#include "dbgutil_win32_dll_event.h"
#endif

namespace dbgutil {

static Logger sLogger;

#ifdef DBGUTIL_WINDOWS
struct TlsCleanupData {
    TlsCleanupData(tlsDestructorFunc dtor, TlsKey key) : m_dtor(dtor), m_key(key) {}
    tlsDestructorFunc m_dtor;
    TlsKey m_key;
};

static void tlsCleanup(int event, void* userData) {
    TlsCleanupData* cleanupData = (TlsCleanupData*)userData;
    if (event == DBGUTIL_DLL_THREAD_DETACH) {
        LOG_TRACE(sLogger, "Running TLS cleanup at %p", cleanupData);
        if (cleanupData == nullptr) {
            // no cleanup data, we give up
            return;
        }
        // get the tls value and call cleanup function
        tlsDestructorFunc dtor = cleanupData->m_dtor;
        void* tlsValue = getTls(cleanupData->m_key);
        if (tlsValue != nullptr) {
            (*dtor)(tlsValue);
            setTls(cleanupData->m_key, nullptr);
        }
    }
}

class TlsKeyPurge : public DllPurgeFilter {
public:
    TlsKeyPurge(TlsKey key) : m_key(key) {}
    TlsKeyPurge(const TlsKeyPurge&) = delete;
    TlsKeyPurge(TlsKeyPurge&&) = delete;
    TlsKeyPurge& operator=(const TlsKeyPurge&) = delete;
    ~TlsKeyPurge() final {}

    bool purge(ThreadDllEventCB /* callback */, void* userData) final {
        TlsCleanupData* cleanupData = (TlsCleanupData*)userData;
        if (cleanupData != nullptr && cleanupData->m_key == m_key) {
            delete cleanupData;
            return true;
        }
        return false;
    }

private:
    TlsKey m_key;
};
#endif

void initTls() { registerLogger(sLogger, "tls"); }

void termTls() { unregisterLogger(sLogger); }

bool createTls(TlsKey& key, tlsDestructorFunc dtor /* = nullptr */) {
#ifdef DBGUTIL_WINDOWS
    key = TlsAlloc();
    if (key == TLS_OUT_OF_INDEXES) {
        LOG_ERROR(sLogger, "Cannot allocate thread local storage slot, out of slots");
        return false;
    }
    if (dtor != nullptr) {
        TlsCleanupData* cleanupData = new (std::nothrow) TlsCleanupData(dtor, key);
        if (cleanupData == nullptr) {
            LOG_ERROR(sLogger,
                      "Failed to allocate thread local storage cleanup data, out of memory");
            destroyTls(key);
            return false;
        }
        registerDllCallback(tlsCleanup, cleanupData);
    }
#else
    int res = pthread_key_create(&key, dtor);
    if (res != 0) {
        LOG_SYS_ERROR_NUM(sLogger, pthread_key_create, res,
                          "Failed to allocate thread local storage slot");
        return false;
    }
#endif
    return true;
}

bool destroyTls(TlsKey key) {
#ifdef DBGUTIL_WINDOWS
    TlsKeyPurge purge(key);
    purgeDllCallback(&purge);
    if (!TlsFree(key)) {
        LOG_WIN32_ERROR(sLogger, TlsFree, "Failed to free thread local storage slot by key %lu",
                        key);
        return false;
    }
#else
    int res = pthread_key_delete(key);
    if (res != 0) {
        LOG_SYS_ERROR_NUM(sLogger, pthread_key_delete, res, "Failed to delete thread local key");
        return false;
    }
#endif
    return true;
}

void* getTls(TlsKey key) {
#ifdef DBGUTIL_WINDOWS
    return TlsGetValue(key);
#else
    return pthread_getspecific(key);
#endif
}

bool setTls(TlsKey key, void* value) {
#ifdef DBGUTIL_WINDOWS
    if (!TlsSetValue(key, value)) {
        LOG_WIN32_ERROR(sLogger, TlsSetValue, "Failed to set thread local storage value");
        return false;
    }
#else
    int res = pthread_setspecific(key, value);
    if (res != 0) {
        LOG_SYS_ERROR(sLogger, pthread_setspecific, "Failed to set thread local storage value");
        return false;
    }
#endif
    return true;
}

}  // namespace dbgutil
