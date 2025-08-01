#ifndef __LIBDBG_ERR_H__
#define __LIBDBG_ERR_H__

// error codes
#define LIBDBG_ERR_OK 0
#define LIBDBG_ERR_NOMEM 1
#define LIBDBG_ERR_INVALID_ARGUMENT 2
#define LIBDBG_ERR_INVALID_STATE 3
#define LIBDBG_ERR_RESOURCE_LIMIT 4
#define LIBDBG_ERR_SYSTEM_FAILURE 5
#define LIBDBG_ERR_NOT_FOUND 6
#define LIBDBG_ERR_INTERNAL_ERROR 7
#define LIBDBG_ERR_EOF 8
#define LIBDBG_ERR_ALREADY_EXISTS 9
#define LIBDBG_ERR_ACCESS_DENIED 10
#define LIBDBG_ERR_END_OF_STREAM 11
#define LIBDBG_ERR_NOT_IMPLEMENTED 12
#define LIBDBG_ERR_DATA_CORRUPT 13
#define LIBDBG_ERR_RESOURCE_BUSY 14

namespace libdbg {

/** @typedef Error type. */
typedef int LibDbgErr;

}  // namespace libdbg

#endif  // __LIBDBG_ERR_H__