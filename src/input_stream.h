#ifndef __INPUT_STREAM_H__
#define __INPUT_STREAM_H__

#include "dbgutil_common.h"

namespace dbgutil {

/** @brief Base abstract class for input stream objects. */
class InputStream {
public:
    /** @brief Resets the input stream (drops all buffers). */
    virtual void reset() = 0;

    /** @brief Queries the stream size.  */
    virtual uint64_t size() const = 0;

    /** @brief Queries whether the stream is empty. */
    inline bool empty() const { return size() == 0; }

    /** @brief Specifies wether bytes arriving from this stream has big endian byte order. */
    bool requiresBigEndian() const { return m_requiresBigEndian; }

    /**
     * @brief Peeks for a few bytes in the stream without pulling them.
     * @note If the stream is depleted, then E_END_OF_STREAM is returned, otherwise the number of
     * possible bytes are peeked and E_OK is returned.
     * @param buffer Received the bytes peek from the stream.
     * @param length The amount of bytes to peek.
     * @param[out] bytesRead The number of bytes actually peeked.
     * @return DbgUtilErr The operation result.
     */
    virtual DbgUtilErr peekBytes(char* buffer, uint32_t length, uint32_t& bytesRead) = 0;

    /**
     * @brief Reads bytes from the stream.
     * @note If the stream is depleted, then E_END_OF_STREAM is returned, otherwise the number of
     * possible bytes are read and E_OK is returned.
     * @param buffer Received the bytes read from the stream.
     * @param length The amount of bytes to read.
     * @param[out] bytesRead The number of bytes actually read.
     * @return DbgUtilErr The operation result.
     */
    virtual DbgUtilErr readBytes(char* buffer, uint32_t length, uint32_t& bytesRead) = 0;

    /**
     * @brief Skips the number of specified bytes in the stream.
     * @note If the stream is depleted, then E_END_OF_STREAM is returned, otherwise the number of
     * possible bytes are skipped and E_OK is returned.
     * @param length The amount of bytes to skip.
     * @param[out] bytesRead The number of bytes actually skipped.
     * @return DbgUtilErr The operation result.
     */
    virtual DbgUtilErr skipBytes(uint32_t length, uint32_t& bytesRead) = 0;

    /**
     * @brief Peeks for a value.
     * @tparam T The value type.
     * @param[out] value The value to peek.
     * @return E_OK If the value was read successfully.
     * @return E_END_OF_STREAM If not enough bytes were present to read the value.
     * @return DbgUtilErr Any other error by the input stream implementation.
     */
    template <typename T>
    inline DbgUtilErr peek(T& value) {
        if (size() < sizeof(T)) {
            return DBGUTIL_ERR_END_OF_STREAM;
        }
        uint32_t length = 0;
        DbgUtilErr rc = peekBytes((char*)&value, sizeof(T), length);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        if (length < sizeof(T)) {
            return DBGUTIL_ERR_END_OF_STREAM;
        }
        return DBGUTIL_ERR_OK;
    }

    /**
     * @brief Reads a value.
     * @tparam T The value type.
     * @param[out] value The value to read.
     * @return E_OK If the value was read successfully.
     * @return E_END_OF_STREAM If not enough bytes were present to read the value.
     * @return DbgUtilErr Any other error by the input stream implementation.
     */
    template <typename T>
    inline DbgUtilErr read(T& value) {
        if (size() < sizeof(T)) {
            return DBGUTIL_ERR_END_OF_STREAM;
        }
        uint32_t length = 0;
        DbgUtilErr rc = readBytes(reinterpret_cast<char*>(&value), sizeof(T), length);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        if (length < sizeof(T)) {
            return DBGUTIL_ERR_END_OF_STREAM;
        }
        return DBGUTIL_ERR_OK;
    }

    /**
     * @brief Reads bytes until a condition is met.
     * @tparam F The bytes consumer function type.
     * @param f The function consuming bytes (one at a time). The function should return false to
     * stop consuming bytes.
     * @return DbgUtilErr The operation result.
     */
    template <typename F>
    inline DbgUtilErr readUntil(F f) {
        uint8_t byte = 0;
        do {
            uint32_t bytesRead = 0;
            DbgUtilErr rc = readBytes((char*)&byte, 1, bytesRead);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
            if (bytesRead == 0) {
                return DBGUTIL_ERR_END_OF_STREAM;
            }
        } while (f(byte));
        return DBGUTIL_ERR_OK;
    }

protected:
    InputStream(bool requiresBigEndian) : m_requiresBigEndian(requiresBigEndian) {}
    virtual ~InputStream() {}

private:
    bool m_requiresBigEndian;
};

}  // namespace dbgutil

#endif  // __INPUT_STREAM_H__