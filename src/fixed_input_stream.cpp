#include "fixed_input_stream.h"

#include <cstring>

namespace dbgutil {

FixedInputStream::FixedInputStream(const char* buffer, size_t bufferSize, bool byRef /* = true */,
                                   bool requiresBigEndian /* = false */)
    : InputStream(requiresBigEndian),
      m_bufRef(byRef ? buffer : nullptr),
      m_size(bufferSize),
      m_offset(0) {
    if (!byRef) {
        m_buf.resize(bufferSize);
        m_bufRef = &m_buf[0];
    }
}

DbgUtilErr FixedInputStream::peekBytes(char* buffer, size_t length, size_t& bytesRead) {
    size_t offset = m_offset;
    DbgUtilErr rc = readBytes(buffer, length, bytesRead);
    m_offset = offset;
    return rc;
}

DbgUtilErr FixedInputStream::readBytes(char* buffer, size_t length, size_t& bytesRead) {
    uint64_t bytesCanRead = size();
    if (bytesCanRead == 0) {
        return DBGUTIL_ERR_END_OF_STREAM;
    }
    bytesRead = std::min(length, bytesCanRead);
    memcpy(buffer, m_bufRef + m_offset, bytesRead);
    m_offset += bytesRead;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr FixedInputStream::skipBytes(size_t length, size_t& bytesRead) {
    size_t bytesCanRead = size();
    if (bytesCanRead == 0) {
        return DBGUTIL_ERR_END_OF_STREAM;
    }
    bytesRead = std::min(length, bytesCanRead);
    m_offset += bytesRead;
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil
