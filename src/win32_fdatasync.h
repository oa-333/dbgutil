#ifndef __WIN32_FDATASYNC_H__
#define __WIN32_FDATASYNC_H__

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

namespace libdbg {

extern int win32FDataSync(int fd);

}  // namespace libdbg

#endif  // LIBDBG_WINDOWS

#endif  // __WIN32_FDATASYNC_H__