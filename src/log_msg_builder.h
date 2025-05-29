#ifndef __LOG_MSG_BUILDER_H__
#define __LOG_MSG_BUILDER_H__

#include <cstdio>

#include "log_buffer.h"

namespace dbgutil {

/** @brief Helper class for safely building log messages. */
class LogMsgBuilder {
public:
    LogMsgBuilder() : m_offset(0), m_bufferFull(false) {}
    ~LogMsgBuilder() {}

    inline uint32_t getOffset() const { return m_offset; }

    /** @brief Finalizes the log record. */
    inline const char* finalize() {
        if (m_bufferFull) {
            // put terminating null in case buffer got full
            m_buffer.getRef()[m_buffer.size() - 1] = 0;
        }
        return m_buffer.getRef();
    }

    /** @brief Resets the log record. */
    inline void reset() {
        m_buffer.reset();
        m_offset = 0;
        m_bufferFull = false;
    }

    /** @brief Appends a formatted message to the log buffer. */
    inline void appendV(const char* fmt, va_list ap) {
        m_offset += vsnprintf(m_buffer.getRef() + m_offset, m_buffer.size() - m_offset, fmt, ap);
    }

    /** @brief Appends a string to the log buffer. */
    inline void append(const char* msg) {
        m_offset += dbgutil_strncpy(m_buffer.getRef() + m_offset, msg, m_buffer.size() - m_offset);
    }

    /** @brief Ensures the log buffer has enough bytes. */
    inline bool ensureBufferLength(uint32_t requiredBytes) {
        bool res = true;
        if (m_buffer.size() - m_offset < requiredBytes) {
            res = m_buffer.resize(m_offset + requiredBytes);
            if (!res) {
                m_bufferFull = true;
            }
        }
        return res;
    }

private:
    LogBuffer m_buffer;
    uint32_t m_offset;
    bool m_bufferFull;

    uint32_t dbgutil_strncpy(char* dest, const char* src, uint32_t dest_len);
};

}  // namespace dbgutil

#endif  // __LOG_MSG_BUILDER_H__