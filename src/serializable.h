#ifndef __SERIALIZABLE_H__
#define __SERIALIZABLE_H__

#include "input_stream.h"
#include "output_stream.h"

#ifdef DBGUTIL_WINDOWS
#include "winsock2.h"
#else
#include <endian.h>
#include <netinet/in.h>
#ifndef htons
#define htons htobe16
#endif
#ifndef htonl
#define htonl htobe32
#endif
#ifndef htonll
#define htonll htobe64
#endif
#ifndef ntohs
#define ntohs be16toh
#endif
#ifndef ntohl
#define ntohl be32toh
#endif
#ifndef ntohll
#define ntohll be64toh
#endif
#endif

namespace libdbg {

/** @brief Serializable interface. */
class Serializable {
public:
    virtual ~Serializable() {}

    /** @brief Serialize this object into an output stream. */
    virtual LibDbgErr serialize(OutputStream& os) = 0;

    /** @brief Deserialize this object from an input stream. */
    virtual LibDbgErr deserialize(InputStream& is) = 0;

protected:
    Serializable() {}
    Serializable(const Serializable&) = delete;
};

/** @brief Serializes 1-byte integer. */
#define DBGUTIL_SERIALIZE_INT8(os, value)    \
    {                                        \
        LibDbgErr rcLocal = os.write(value); \
        if (rcLocal != LIBDBG_ERR_OK) {      \
            return rcLocal;                  \
        }                                    \
    }

/** @brief Serializes 2-byte integer. */
#define DBGUTIL_SERIALIZE_INT16(os, value)                                           \
    {                                                                                \
        LibDbgErr rcLocal = os.write(os.requiresBigEndian() ? htons(value) : value); \
        if (rcLocal != LIBDBG_ERR_OK) {                                              \
            return rcLocal;                                                          \
        }                                                                            \
    }

/** @brief Serializes 4-byte integer. */
#define DBGUTIL_SERIALIZE_INT32(os, value)                                           \
    {                                                                                \
        LibDbgErr rcLocal = os.write(os.requiresBigEndian() ? htonl(value) : value); \
        if (rcLocal != LIBDBG_ERR_OK) {                                              \
            return rcLocal;                                                          \
        }                                                                            \
    }

/** @brief Serializes 8-byte integer. */
#define DBGUTIL_SERIALIZE_INT64(os, value)                                            \
    {                                                                                 \
        LibDbgErr rcLocal = os.write(os.requiresBigEndian() ? htonll(value) : value); \
        if (rcLocal != LIBDBG_ERR_OK) {                                               \
            return rcLocal;                                                           \
        }                                                                             \
    }

/** @brief Serializes boolean value. */
#define DBGUTIL_SERIALIZE_BOOL(os, value)     \
    {                                         \
        uint8_t value8 = value ? 1 : 0;       \
        LibDbgErr rcLocal = os.write(value8); \
        if (rcLocal != LIBDBG_ERR_OK) {       \
            return rcLocal;                   \
        }                                     \
    }

/** @brief Serializes enumerated value. */
#define DBGUTIL_SERIALIZE_ENUM(os, value)                       \
    {                                                           \
        switch (sizeof(value)) {                                \
            case 1:                                             \
                DBGUTIL_SERIALIZE_INT8(os, (uint8_t)(value));   \
                break;                                          \
            case 2:                                             \
                DBGUTIL_SERIALIZE_INT16(os, (uint16_t)(value)); \
                break;                                          \
            case 4:                                             \
                DBGUTIL_SERIALIZE_INT32(os, (uint32_t)(value)); \
                break;                                          \
            case 8:                                             \
                DBGUTIL_SERIALIZE_INT64(os, (uint64_t)(value)); \
                break;                                          \
            default:                                            \
                return LIBDBG_ERR_INVALID_ARGUMENT;             \
        }                                                       \
    }

// TODO: this does not take care of byte order
/** @brief Serializes generic data (e.g. flat struct. No byte ordering takes place). */
#define DBGUTIL_SERIALIZE_DATA(os, value)    \
    {                                        \
        LibDbgErr rcLocal = os.write(value); \
        if (rcLocal != LIBDBG_ERR_OK) {      \
            return rcLocal;                  \
        }                                    \
    }

/** @def Serializes a serializable object. */
#define DBGUTIL_SERIALIZE(os, value)               \
    {                                              \
        LibDbgErr rcLocal = (value).serialize(os); \
        if (rcLocal != LIBDBG_ERR_OK) {            \
            return rcLocal;                        \
        }                                          \
    }

/** @def Serializes length-prepended std::string. */
#define DBGUTIL_SERIALIZE_STRING(os, value)                               \
    {                                                                     \
        DBGUTIL_SERIALIZE_INT32(os, (uint32_t)value.length());            \
        LibDbgErr rcLocal = os.writeBytes(value.c_str(), value.length()); \
        if (rcLocal != LIBDBG_ERR_OK) {                                   \
            return rcLocal;                                               \
        }                                                                 \
    }

/** @def Serializes null-terminated string. */
#define DBGUTIL_SERIALIZE_NT_STRING(os, value, len)    \
    {                                                  \
        LibDbgErr rcLocal = os.writeBytes(value, len); \
        if (rcLocal != LIBDBG_ERR_OK) {                \
            return rcLocal;                            \
        }                                              \
        DBGUTIL_SERIALIZE_INT8(os, 0);                 \
    }

/** @brief Deserializes 1-byte integer value. */
#define DBGUTIL_DESERIALIZE_INT8(is, value) \
    {                                       \
        LibDbgErr rcLocal = is.read(value); \
        if (rcLocal != LIBDBG_ERR_OK) {     \
            return rcLocal;                 \
        }                                   \
    }

/** @brief Deserializes 2-byte integer value. */
#define DBGUTIL_DESERIALIZE_INT16(is, value) \
    {                                        \
        LibDbgErr rcLocal = is.read(value);  \
        if (rcLocal != LIBDBG_ERR_OK) {      \
            return rcLocal;                  \
        }                                    \
        if (is.requiresBigEndian()) {        \
            value = ntohs(value);            \
        }                                    \
    }

/** @brief Deserializes 4-byte integer value. */
#define DBGUTIL_DESERIALIZE_INT32(is, value) \
    {                                        \
        LibDbgErr rcLocal = is.read(value);  \
        if (rcLocal != LIBDBG_ERR_OK) {      \
            return rcLocal;                  \
        }                                    \
        if (is.requiresBigEndian()) {        \
            value = ntohl(value);            \
        }                                    \
    }

/** @brief Deserializes 8-byte integer value. */
#define DBGUTIL_DESERIALIZE_INT64(is, value) \
    {                                        \
        LibDbgErr rcLocal = is.read(value);  \
        if (rcLocal != LIBDBG_ERR_OK) {      \
            return rcLocal;                  \
        }                                    \
        if (is.requiresBigEndian()) {        \
            value = ntohll(value);           \
        }                                    \
    }

/** @brief Deserializes boolean value. */
#define DBGUTIL_DESERIALIZE_BOOL(is, value)  \
    {                                        \
        uint8_t value8 = 0;                  \
        LibDbgErr rcLocal = is.read(value8); \
        if (rcLocal != LIBDBG_ERR_OK) {      \
            return rcLocal;                  \
        }                                    \
        value = (value8 != 0);               \
    }

/** @brief Deserializes enumerated value. */
#define DBGUTIL_DESERIALIZE_ENUM(is, value)                        \
    {                                                              \
        switch (sizeof(value)) {                                   \
            case 1:                                                \
                DBGUTIL_DESERIALIZE_INT8(is, (uint8_t&)(value));   \
                break;                                             \
            case 2:                                                \
                DBGUTIL_DESERIALIZE_INT16(is, (uint16_t&)(value)); \
                break;                                             \
            case 4:                                                \
                DBGUTIL_DESERIALIZE_INT32(is, (uint32_t&)(value)); \
                break;                                             \
            case 8:                                                \
                DBGUTIL_DESERIALIZE_INT64(is, (uint64_t&)(value)); \
                break;                                             \
            default:                                               \
                return LIBDBG_ERR_INVALID_ARGUMENT;                \
        }                                                          \
    }

/** @brief Deserializes generic data (e.g. falt struct. No byte ordering takes place). */
#define DBGUTIL_DESERIALIZE_DATA(is, value) \
    {                                       \
        LibDbgErr rcLocal = is.read(value); \
        if (rcLocal != LIBDBG_ERR_OK) {     \
            return rcLocal;                 \
        }                                   \
    }

/** @brief Deserializes serializable object. */
#define DBGUTIL_DESERIALIZE(is, value)               \
    {                                                \
        LibDbgErr rcLocal = (value).deserialize(is); \
        if (rcLocal != LIBDBG_ERR_OK) {              \
            return rcLocal;                          \
        }                                            \
    }

/** @brief Deserializes length-prepended string into std::string. */
#define DBGUTIL_DESERIALIZE_STRING(is, value)                         \
    {                                                                 \
        uint32_t length = 0;                                          \
        DBGUTIL_DESERIALIZE_INT32(is, length);                        \
        std::vector<char> buf(length + 1, 0);                         \
        uint32_t bytesRead = 0;                                       \
        LibDbgErr rcLocal = is.readBytes(&buf[0], length, bytesRead); \
        if (rcLocal != LIBDBG_ERR_OK) {                               \
            return rcLocal;                                           \
        }                                                             \
        value = (const char*)&buf[0];                                 \
    }

/** @brief Deserializes null-terminated string into std::string. */
#define DBGUTIL_DESERIALIZE_NT_STRING(is, value)                  \
    {                                                             \
        LibDbgErr rcLocal = is.readUntil([&value](uint8_t byte) { \
            if (byte == 0) {                                      \
                return false;                                     \
            }                                                     \
            value += (char)byte;                                  \
            return true;                                          \
        });                                                       \
        if (rcLocal != LIBDBG_ERR_OK) {                           \
            return rcLocal;                                       \
        }                                                         \
    }

}  // namespace libdbg

#endif  // __SERIALIZABLE_H__