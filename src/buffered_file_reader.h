#ifndef __BUFFERED_FILE_READER_H__
#define __BUFFERED_FILE_READER_H__

#include <vector>

#include "dbgutil_common.h"

namespace dbgutil {

/** @brief A buffered file reader. */
class DBGUTIL_API BufferedFileReader {
public:
    BufferedFileReader();
    ~BufferedFileReader() {}

    static void initLogger();

    /** @brief The default buffer size. */
    static const uint32_t DEFAULT_BUFFER_SIZE;

    /**
     * @brief Opens the reader over a file.
     * @param filePath The path of the file to read.
     * @param bufferSize The buffer size used in reading.
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr open(const char* filePath, uint32_t bufferSize = DEFAULT_BUFFER_SIZE);

    /** @brief Closed the buffered reader. */
    DbgUtilErr close();

    /** @brief Queries whether the reader is open. */
    inline bool isOpen() const { return m_fd != 0; }

    /** @brief Retrieves the current offset (from begining of file) of the reader. */
    DbgUtilErr getOffset(uint64_t& offset) const;

    /** @brief Sets file pointer to a specified position (offset from begining of file). */
    DbgUtilErr seek(uint64_t offset);

    /** @brief Queries whether the reader has reached the end of the file. */
    inline bool eof() const { return m_eof; }

    /**
     * @brief Reads a typed value.
     * @tparam T The type of the value.
     * @param[out] value The resulting value.
     * @return DbgUtilErr The operation result.
     */
    template <typename T>
    inline DbgUtilErr read(T& value) {
        return readFull((char*)&value, sizeof(T));
    }

    /**
     * @brief Reads fully a given amount of bytes.
     * @param buffer The buffer receiving the read data.
     * @param len The number of bytes to read.
     * @param[out] bytesReadRef Optionally receives the number of bytes actually read.
     * @return E_OK If the operation succeeded and all bytes were read. (note this is different than
     * @ref read()).
     * @return E_EOF If not all bytes could be read due to end of file. If bytesReadRef was passed,
     * then it will contain the actual number of bytes read. (note this is different than @ref
     * read()).
     * @return DbgUtilErr Any other error.
     */
    DbgUtilErr readFull(char* buffer, uint32_t len, uint32_t* bytesReadRef = nullptr);

    /**
     * @brief Reads data from the buffered file reader.
     * @param buffer The buffer into which data is to be read.
     * @param len The buffer length.
     * @param bytesRead[out] The actual number of bytes read.
     * @return E_OK If the operation succeeded. The amount of bytes is put in bytesRead. This can be
     * less the requested amount of bytes to read, in case end of file was reached. Note that in
     * this case E_EOF is not returned.
     * @return E_EOF If end of file has already been reached prior to this call. This indicates no
     * bytes were read at all (bytesRead is returned with value 0).
     * @return DbgUtilErr Any other error code.
     */
    DbgUtilErr read(char* buffer, uint32_t len, uint32_t& bytesRead);

    /**
     * @brief Skips the number of specified bytes in the buffered file reader.
     * @note If the file is exhausted, then E_EOF is returned, otherwise the number of
     * possible bytes are skipped and E_OK is returned.
     * @param length The amount of bytes to skip.
     * @return DbgUtilErr The operation result.
     */
    inline DbgUtilErr skip(uint32_t length) { return seek(m_fileOffset + m_bufferOffset + length); }

private:
    int m_fd;
    uint64_t m_fileOffset;
    uint32_t m_bufferSize;
    std::vector<char> m_buffer;
    uint32_t m_bufferOffset;
    bool m_eof;

    DbgUtilErr refillBuffer();
};

}  // namespace dbgutil

#endif  // __BUFFERED_FILE_READER_H__