#if 0
#include "win32_fdatasync.h"

// we must be careful here, because we need ot have Windows Driver Kit headers included as well
// so to avoid conflict, we must avoid including windows headers
#define DBGUTIL_NO_WINDOWS_HEADER
#include "libdbg_def.h"

// no we can include headers in the correct order
// first include WDK headers (enclosed in ifdef to avoid code formatting doing reordering)

#ifdef DBGUTIL_WINDOWS
// #define _AMD64_
#include <sdkddkver.h>
#endif

#ifdef DBGUTIL_WINDOWS
#define _AMD64_
#include <ntddk.h>
//  #include <wdf.h>
#endif

#ifdef DBGUTIL_WINDOWS
// #include <ntifs.h>
#endif

// now include windows headers
#ifdef DBGUTIL_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// #include <windows.h>
#endif

#ifdef DBGUTIL_WINDOWS
#include <errno.h>
#include <io.h>
// #include <winternl.h>
#endif

namespace libdbg {

static int initialize_ntdll() { return 0; };

int win32FDataSync(int fd) {
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    if (handle == (HANDLE)-1) {
        return EBADF;
    }

    if (initialize_ntdll() < 0) {
        return EFAULT;  //  TODO: is this meaningful?
    }

    IO_STATUS_BLOCK ioStatBlock = {};
    memset(&ioStatBlock, 0, sizeof(&ioStatBlock));
    NTSTATUS status =
        NtFlushBuffersFileEx(handle, FLUSH_FLAGS_FILE_DATA_SYNC_ONLY, NULL, 0, &ioStatBlock);
    if (NT_SUCCESS(status)) {
        return 0;
    }

    // TODO: this should be fixed
    return EFAULT;  //  TODO: is this meaningful?
    /*ULONG errCode = RtlNtStatusToDosError(status);
    SetLastError(errCode);
    rc = LIBDBG_ERR_SYSTEM_FAILURE;
    errno = EFAULT;  //  TODO: is this meaningful?*/
    /*const std::error_condition errCond =
        std::system_category().default_error_condition(errCode);
    return errCond.value();*/
}

}  // namespace libdbg
#endif