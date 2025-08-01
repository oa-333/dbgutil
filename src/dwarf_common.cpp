#include "dwarf_common.h"

#include "dwarf_def.h"

namespace libdbg {

DbgUtilErr dwarfReadInitialLength(InputStream& is, uint64_t& len, bool& is64Bit) {
    uint32_t lenPrefix = 0;
    DBGUTIL_DESERIALIZE_INT32(is, lenPrefix);
    if (len < 0xffffff00) {
        len = lenPrefix;
        is64Bit = false;
    } else {
        if (len != 0xffffffff) {
            return DBGUTIL_ERR_DATA_CORRUPT;
        }
        DBGUTIL_DESERIALIZE_INT64(is, len);
        is64Bit = true;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr dwarfReadOffset(InputStream& is, uint64_t& offset, bool is64Bit) {
    if (is64Bit) {
        DBGUTIL_DESERIALIZE_INT64(is, offset);
    } else {
        uint32_t offset32 = 0;
        DBGUTIL_DESERIALIZE_INT32(is, offset32);
        offset = offset32;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr dwarfReadAddress(InputStream& is, uint64_t& offset, uint64_t addressSize) {
    if (addressSize == 8) {
        DBGUTIL_DESERIALIZE_INT64(is, offset);
    } else if (addressSize == 4) {
        uint32_t offset32 = 0;
        DBGUTIL_DESERIALIZE_INT32(is, offset32);
        offset = offset32;
    } else {
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr dwarfReadULEB128(InputStream& is, uint64_t& result) {
    result = 0;
    uint32_t shift = 0;
    while (true) {
        // read next byte from input
        uint8_t byte = 0;
        DBGUTIL_DESERIALIZE_INT8(is, byte);

        // select lower 7 bits, and put in 64 bit value
        uint64_t byte64 = (byte & ((uint64_t)0x7F));

        // shift next byte and add to result
        result |= (byte64 << shift);
        shift += 7;

        // stop if high order bit of the input byte is zero
        if ((byte & 0x80) == 0) {
            break;
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr dwarfReadSLEB128(InputStream& is, int64_t& result) {
    result = 0;
    uint32_t shift = 0;
    bool signBitSet = 0;

    // this number is the bit-size of the output type we use for decoding result
    uint32_t size = 64;
    while (true) {
        // read next byte from input
        uint8_t byte = 0;
        DBGUTIL_DESERIALIZE_INT8(is, byte);

        // select lower 7 bits, and put in 64 bit value
        int64_t byte64 = (byte & 0x7F);

        // shift next byte and add to result
        result |= (byte64 << shift);
        shift += 7;

        // stop if high order bit of the input byte is zero
        if ((byte & 0x80) == 0) {
            // remember sign bit status
            // (sign bit of byte is second high order bit, so mask is 0x40)
            signBitSet = ((byte & 0x40) != 0);
            break;
        }
    }

    // check for sign extension
    if ((shift < size) && signBitSet) {
        result |= -(1 << shift);
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr dwarfReadString(InputStream& is, uint64_t form, bool is64Bit, DwarfData& dwarfData,
                           std::string& result) {
    if (form == DW_FORM_string) {
        // read string directly from input stream
        DBGUTIL_DESERIALIZE_NT_STRING(is, result);
    } else if (form == DW_FORM_strp) {
        // read offset from input stream, then read for .debug_str section
        uint64_t strOffset = 0;
        DWARF_READ_OFFSET(is, strOffset, is64Bit);
        const DwarfSection& section = dwarfData.getDebugStr();
        if (strOffset >= section.m_size) {
            return DBGUTIL_ERR_DATA_CORRUPT;
        }
        result = (char*)section.m_start + strOffset;
    } else if (form == DW_FORM_line_strp) {
        // read offset from input stream, then read for .debug_line_str section
        uint64_t strOffset = 0;
        DWARF_READ_OFFSET(is, strOffset, is64Bit);
        const DwarfSection& section = dwarfData.getDebugLineStr();
        if (strOffset >= section.m_size) {
            return DBGUTIL_ERR_DATA_CORRUPT;
        }
        result = (char*)section.m_start + strOffset;
    } else {
        // other forms are not supported
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }
    return DBGUTIL_ERR_OK;
}

bool DwarfData::checkDebugSections() {
    return getSection(".debug_info", m_debugInfo) &&
           getSection(".debug_aranges", m_debugAddrRanges) &&
           getSection(".debug_line", m_debugLine) && getSection(".debug_str", m_debugStr) &&
           getSection(".debug_line_str", m_debugLineStr) &&
           getSection(".debug_abbrev", m_debugAbbrev) &&
           getSection(".debug_rnglists",
                      m_debugRngLists) /*&& getSection(".debug_addr", m_debugAddr)*/;
}

}  // namespace libdbg
