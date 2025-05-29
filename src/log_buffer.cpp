#include "log_buffer.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace dbgutil {

LogBuffer::~LogBuffer() {
#ifndef __MINGW32__
    reset();
#endif
}

bool LogBuffer::resize(uint32_t newSize) {
    if (m_bufferSize < newSize) {
        bool shouldCopy = (m_dynamicBuffer == nullptr);
        char* newBuffer = (char*)realloc(m_dynamicBuffer, newSize);
        if (newBuffer == nullptr) {
            return false;
        }
        m_dynamicBuffer = newBuffer;
        if (shouldCopy) {
            strncpy(m_dynamicBuffer, m_fixedBuffer, m_bufferSize);
        }
        m_bufferSize = newSize;
    }
    return true;
}

void LogBuffer::reset() {
    if (m_dynamicBuffer != nullptr) {
        free(m_dynamicBuffer);
        m_dynamicBuffer = nullptr;
    }
    m_bufferSize = LOG_BUFFER_SIZE;
}

}  // namespace dbgutil
