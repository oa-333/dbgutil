#ifndef __OUTPUT_STREAM_H__
#define __OUTPUT_STREAM_H__

#include "dbgutil_common.h"

namespace dbgutil {

/** @brief Base abstract class for output stream objects. */
class DBGUTIL_API OutputStream {
public:
    /**
     * @brief Writes a value.
     * @tparam T The value type.
     * @param value The value to write.
     * @return true If enough bytes were present to read the value.
     * @return false If not enough bytes were present to read the value.
     */
    template <typename T>
    inline DbgUtilErr write(const T& value) {
        return writeBytes((const char*)&value, sizeof(T));
    }

    /**
     * @brief Writes a buffer to the output stream.
     * @param buffer The buffer to write.
     * @param length The length of the buffer.
     * @return DbgUtilErr The operation result.
     */
    virtual DbgUtilErr writeBytes(const char* buffer, uint32_t length) = 0;

    /** @brief Specifies whether bytes sent through this stream require big endian byte order. */
    bool requiresBigEndian() const { return m_requiresBigEndian; }

protected:
    OutputStream(bool requiresBigEndian) : m_requiresBigEndian(requiresBigEndian) {}
    virtual ~OutputStream() {}

private:
    bool m_requiresBigEndian;
};

}  // namespace dbgutil

#endif  // __OUTPUT_STREAM_H__