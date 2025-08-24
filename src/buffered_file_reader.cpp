#include "buffered_file_reader.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include "dbgutil_log_imp.h"
#include "os_util.h"

namespace dbgutil {

static Logger sLogger;

void BufferedFileReader::initLogger() { registerLogger(sLogger, "buffered_file_reader"); }
void BufferedFileReader::termLogger() { unregisterLogger(sLogger); }

const size_t BufferedFileReader::DEFAULT_BUFFER_SIZE = 4096;

BufferedFileReader::BufferedFileReader()
    : m_fd(0), m_fileOffset(0), m_bufferSize(0), m_bufferOffset(0), m_eof(false) {}

DbgUtilErr BufferedFileReader::open(const char* filePath,
                                    size_t bufferSize /* = BFR_DEFAULT_BUFFER_SIZE */) {
    if (isOpen()) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    DbgUtilErr rc = OsUtil::openFile(filePath, O_BINARY | O_RDONLY, 0, m_fd);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to open file %s for binary reading: %s", filePath,
                  errorToString(rc));
        return rc;
    }
    m_fileOffset = 0;
    m_buffer.clear();
    m_bufferSize = bufferSize;
    m_bufferOffset = 0;
    return refillBuffer();
}

DbgUtilErr BufferedFileReader::close() {
    if (!isOpen()) {
        return DBGUTIL_ERR_INVALID_STATE;
    }
    DbgUtilErr rc = OsUtil::closeFile(m_fd);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to close file: %s", errorToString(rc));
        return rc;
    }
    m_fd = 0;
    m_fileOffset = 0;
    m_buffer.clear();
    m_bufferSize = 0;
    m_bufferOffset = 0;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr BufferedFileReader::getOffset(size_t& offset) const {
    // check state
    if (!isOpen()) {
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // calculate offset
    offset = m_fileOffset + m_bufferOffset;
    return DBGUTIL_ERR_OK;
}

DbgUtilErr BufferedFileReader::seek(size_t offset) {
    // check state
    if (!isOpen()) {
        return DBGUTIL_ERR_INVALID_STATE;
    }

    // if the offset is found inside the current buffer, then we don't need to seek, but rather
    // just update the buffer offset
    if (offset >= m_fileOffset && offset < (m_fileOffset + m_buffer.size())) {
        m_bufferOffset = offset - m_fileOffset;
        return DBGUTIL_ERR_OK;
    }

    // NOTE: be careful here, since seekFile() excepts signed 64 bit value, but offset is unsigned,
    // so we must check for overflow, otherwise cast is wrong
    if (offset > INT64_MAX) {
        LOG_ERROR(sLogger, "Request to seek file to offset %zu declined, offset too large", offset);
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    // otherwise we must seek to the required offset and refill buffer
    DbgUtilErr rc = OsUtil::seekFile(m_fd, (int64_t)offset, SEEK_SET);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_ERROR(sLogger, "Failed to seek to offset %" PRIu64, offset);
        return rc;
    }

    // reset file offset to given offset
    m_fileOffset = offset;

    // also buffer must be discarded, otherwise refillBuffer() will set wrong file offset
    m_buffer.clear();
    m_bufferOffset = 0;

    // we need to refill buffer from new position
    return refillBuffer();
}

DbgUtilErr BufferedFileReader::readFull(char* buffer, size_t len,
                                        size_t* bytesReadRef /* = nullptr */) {
    // NOTE: no state check, as this might be called frequently
    size_t bytesRead = 0;
    DbgUtilErr rc = read(buffer, len, bytesRead);
    if (bytesReadRef != nullptr) {
        *bytesReadRef = bytesRead;
    }
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    if (bytesRead < len) {
        m_eof = true;
        return DBGUTIL_ERR_EOF;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr BufferedFileReader::read(char* buffer, size_t len, size_t& bytesRead) {
    // NOTE: no state check, as this might be called frequently
    bytesRead = 0;
    while (bytesRead < len) {
        // make sure buffer has some bytes ready
        if (m_bufferOffset == m_buffer.size()) {
            DbgUtilErr rc = refillBuffer();
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
        }
        size_t bytesToRead = len - bytesRead;
        size_t bytesInBuffer = m_buffer.size() - m_bufferOffset;
        size_t bytesCanRead = std::min(bytesInBuffer, bytesToRead);
        memcpy(buffer + bytesRead, &m_buffer[m_bufferOffset], bytesCanRead);
        bytesRead += bytesCanRead;
        m_bufferOffset += bytesCanRead;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr BufferedFileReader::refillBuffer() {
    // first update file offset, and prepare buffer for reading
    m_fileOffset += m_buffer.size();
    m_buffer.resize(m_bufferSize);
    m_bufferOffset = 0;

    // now read another buffer
    size_t bytesRead = 0;
    int sysErr = 0;
    DbgUtilErr rc = OsUtil::readFile(m_fd, &m_buffer[0], m_bufferSize, bytesRead, &sysErr);
    if (rc != DBGUTIL_ERR_OK) {
        LOG_SYS_ERROR_NUM(sLogger, read, sysErr, "Failed to refill buffer with %u bytes form file",
                          m_bufferSize);
        return rc;
    }

    // first resize buffer to number of bytes read
    m_buffer.resize(bytesRead);
    if (bytesRead == 0) {
        return DBGUTIL_ERR_EOF;
    }

    // NOTE: receiving less bytes than we asked for does NOT necessarily indicate end of file
    // only zero bytes read indicates end of file
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil