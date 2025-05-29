#ifndef __FIXED_INPUT_STREAM_H__
#define __FIXED_INPUT_STREAM_H__

#include <vector>

#include "input_stream.h"

namespace dbgutil {

/** @brief Input stream over a given fixed buffer. */
class DBGUTIL_API FixedInputStream : public InputStream {
public:
    /**
     * @brief Construct a new fixed input stream object.
     * @param buffer The buffer over which the input stream is built.
     * @param bufferSize The buffer size.
     * @param byRef Optionally specifies whether a reference to the buffer should be used. If false,
     * then the buffer is copied.
     * @param requiresBigEndian Optionally specifies whether the buffer data is usign big endian
     * byte order.
     */
    FixedInputStream(const char* buffer, uint32_t bufferSize, bool byRef = true,
                     bool requiresBigEndian = false);
    ~FixedInputStream() override {}

    /** @brief Retrieves the current offset of the stream. */
    inline uint32_t getOffset() const { return m_offset; }

    /** @brief Resets the input stream (drops all buffers). */
    void reset() final {}

    /** @brief Queries the stream size (how many bytes left to read). */
    uint64_t size() const final { return m_size - m_offset; }

    /**
     * @brief Peeks for a few bytes in the stream without pulling them.
     * @note If the stream is depleted, then E_END_OF_STREAM is returned, otherwise the number of
     * possible bytes are peeked and E_OK is returned.
     * @param buffer Received the bytes peek from the stream.
     * @param length The amount of bytes to peek.
     * @param[out] bytesRead The number of bytes actually peeked.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr peekBytes(char* buffer, uint32_t length, uint32_t& bytesRead) override;

    /**
     * @brief Reads bytes from the stream.
     * @note If the stream is depleted, then E_END_OF_STREAM is returned, otherwise the number of
     * possible bytes are read and E_OK is returned.
     * @param buffer Received the bytes read from the stream.
     * @param length The amount of bytes to read.
     * @param[out] bytesRead The number of bytes actually read.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr readBytes(char* buffer, uint32_t length, uint32_t& bytesRead) override;

    /**
     * @brief Skips the number of specified bytes in the stream.
     * @note If the stream is depleted, then E_END_OF_STREAM is returned, otherwise the number of
     * possible bytes are skipped and E_OK is returned.
     * @param length The amount of bytes to skip.
     * @param[out] bytesRead The number of bytes actually skipped.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr skipBytes(uint32_t length, uint32_t& bytesRead) override;

private:
    std::vector<char> m_buf;
    const char* m_bufRef;
    uint32_t m_size;
    uint32_t m_offset;
};

}  // namespace dbgutil

#endif  // __FIXED_INPUT_STREAM_H__