#ifndef __LOG_BUFFER_H__
#define __LOG_BUFFER_H__

#include <cassert>
#include <cstdint>

#include "dbg_util_def.h"

namespace dbgutil {

/** @def The fixed buffer size used for logging. */
#define LOG_BUFFER_SIZE 1024

/**
 * @brief A fixed size buffer that may transition to dynamic size buffer. This is required by the
 * logger, which maintains a thread local buffer. Under MinGW this causes crash during application
 * shutdown, since for some reason, TLS destructors being run during DLL unload cannot deallocate
 * buffers when calling free. It is unclear whether there is a collision with the C runtime code.
 * Using a fixed buffer (so we avoid calling free() in buffer's destructor) is a bit limiting. So a
 * fixed buffer is used, but if more space is needed by some long log message, it transitions to a
 * dynamic buffer, and the logger is required to release it as soon as it finished logging.
 */
class LogBuffer {
public:
    /** @brief Constructor. */
    LogBuffer() : m_dynamicBuffer(nullptr), m_bufferSize(LOG_BUFFER_SIZE) {}

    /** @brief Destructor. */
    ~LogBuffer();

    /** @brief Returns a reference to the internal buffer. */
    inline char* getRef() { return m_dynamicBuffer ? m_dynamicBuffer : (char*)m_fixedBuffer; }

    /** @brief Returns a reference to the internal buffer. */
    inline const char* getRef() const {
        return m_dynamicBuffer ? m_dynamicBuffer : (const char*)m_fixedBuffer;
    }

    /** @brief Retrieves the current capacity of the buffer. */
    inline uint32_t size() const { return m_bufferSize; }

    /**
     * @brief Increases the current capacity of the buffer. If the buffer's size is already
     * greater than the required size then no action takes place.
     * @param newSize The required new size.
     * @return true If operation succeeded, otherwise false.
     */
    bool resize(uint32_t newSize);

    /** @brief Resets the buffer to original state. Releases the dynamic buffer if needed. */
    void reset();

private:
    char m_fixedBuffer[LOG_BUFFER_SIZE];
    char* m_dynamicBuffer;
    uint32_t m_bufferSize;
};

}  // namespace dbgutil

#endif  // __LOG_BUFFER_H__