#include "dwarf_util.h"

#include <algorithm>
#include <cinttypes>

#include "dbgutil_log_imp.h"
#include "dwarf_def.h"
#include "dwarf_line_util.h"
#include "fixed_input_stream.h"

namespace dbgutil {

static Logger sLogger;

void DwarfUtil::initLogger() { registerLogger(sLogger, "dwarf_util"); }
void DwarfUtil::termLogger() { unregisterLogger(sLogger); }

DwarfUtil::DwarfUtil() {}

DbgUtilErr DwarfUtil::readAddrRangeHeader(InputStream& is, uint64_t& len, bool& is64Bit,
                                          uint64_t& offset, uint8_t& addressSize) {
    // read initial length
    is64Bit = false;
    DWARF_READ_INIT_LEN(is, len, is64Bit);

    // read version (uhalf - unsigned, 2-byte integer)
    // currently only version 3 is supported
    uint16_t version = 0;
    DBGUTIL_DESERIALIZE_INT16(is, version);
    if (version != 2) {
        LOG_DEBUG(sLogger, "ERROR: Address range header version %u not supported",
                  (unsigned)version);
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    // offset into debug info
    DWARF_READ_OFFSET(is, offset, is64Bit);

    // address size (ubyte - unsigned, 1-byte integer)
    DBGUTIL_DESERIALIZE_INT8(is, addressSize);

    // segment size (ubyte - unsigned, 1-byte integer)
    uint8_t segmentSize = 0;
    DBGUTIL_DESERIALIZE_INT8(is, segmentSize);
    if (segmentSize != 0) {
        LOG_DEBUG(sLogger, "ERROR: Segmented address (segment size %u) not supported",
                  (unsigned)segmentSize);
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfUtil::readCUHeader(InputStream& is, uint64_t& len, uint64_t& abbrevOffset,
                                   uint8_t& addressSize, bool& is64Bit) {
    // read initial length
    len = 0;
    is64Bit = false;
    DWARF_READ_INIT_LEN(is, len, is64Bit);

    // read version (uhalf - unsigned, 2-byte integer)
    // currently only version 3 is supported
    uint16_t version = 0;
    DBGUTIL_DESERIALIZE_INT16(is, version);
    if (version < 3) {
        LOG_DEBUG(sLogger, "ERROR: Compilation unit header version %u not supported",
                  (unsigned)version);
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    if (version == 3) {
        // offset into debug abbrev
        DWARF_READ_OFFSET(is, abbrevOffset, is64Bit);

        // address size (ubyte - unsigned, 1-byte integer)
        DBGUTIL_DESERIALIZE_INT8(is, addressSize);
    } else if (version == 5) {
        uint8_t unitType = 0;
        DBGUTIL_DESERIALIZE_INT8(is, unitType);
        if (unitType != DW_UT_compile) {
            return DBGUTIL_ERR_DATA_CORRUPT;
        }

        // address size (ubyte - unsigned, 1-byte integer)
        DBGUTIL_DESERIALIZE_INT8(is, addressSize);

        // offset into debug abbrev
        DWARF_READ_OFFSET(is, abbrevOffset, is64Bit);
    } else {
        // version 4 not supported yet
        LOG_DEBUG(sLogger, "ERROR: Compilation unit header version 4 not supported");
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfUtil::open(const DwarfData& dwarfData, void* moduleBase, bool is664Bit,
                           bool isExe) {
    m_dwarfData = dwarfData;
    m_moduleBase = moduleBase;
    m_is64Bit = is664Bit;
    m_isExe = isExe;
    return buildRangeCuMap();
}

DbgUtilErr DwarfUtil::searchSymbol(void* symAddress, SymbolInfo& symbolInfo, void* relocationBase) {
    // for executable images the address is already ok, but for shared objects it needs to be
    // translated to offset
    uint64_t symOff = (uint64_t)symAddress - (uint64_t)m_moduleBase;
    uint64_t relocSymAddr = ((uint64_t)relocationBase) + symOff;
    DwarfSearchData searchData = {symAddress, symbolInfo.m_moduleBaseAddress, symOff,
                                  relocationBase, relocSymAddr};

    // now search in range map the relocated address (as it appears when debug info was prepared)
    LOG_DEBUG(sLogger, "Searching for relocated address: %p", (void*)relocSymAddr);
    RangeCuMultiMap::iterator itr = m_rangeCuMultiMap.lower_bound(relocSymAddr);
    if (itr == m_rangeCuMultiMap.end()) {
        return DBGUTIL_ERR_NOT_FOUND;
    }

    const AddrRange& rangeData = itr->first;
    if (rangeData.contains((uint64_t)relocSymAddr)) {
        // return searchLineProg(symOff, cuData.m_lineProgOffset, symName, fileName, line);
        // search in all listed offsets
        OffsetSet& offsetSet = itr->second;
        for (uint64_t offset : offsetSet) {
            DbgUtilErr rc = searchSymbolInCU(searchData, rangeData.m_debugInfoOffset, symbolInfo);
            if (rc == DBGUTIL_ERR_OK) {
                return rc;
            }
            if (rc != DBGUTIL_ERR_NOT_FOUND) {
                return rc;
            }
        }
    }
    return DBGUTIL_ERR_NOT_FOUND;
}

DbgUtilErr DwarfUtil::readCUData(uint64_t offset, CUData& cuData) {
    const DwarfSection& debugInfoSection = m_dwarfData.getDebugInfo();
    FixedInputStream is(debugInfoSection.m_start + offset, debugInfoSection.m_size - offset);

    // read CU header
    uint64_t len = 0;
    uint64_t abbrevOffset = 0;
    bool is64Bit = false;
    uint8_t addressSize = 0;
    DbgUtilErr rc = readCUHeader(is, len, abbrevOffset, addressSize, is64Bit);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // we don't read entire CU debug entry tree, but only top level CU tag
    // first read abbreviation code
    uint64_t abbrevCode = 0;
    DWARF_READ_ULEB128(is, abbrevCode);

    // now read entry descriptor from abbreviation table
    uint64_t tag = 0;
    bool hasChildren = false;
    AttrList attrs;
    rc = readAbbrevDecl(abbrevOffset, abbrevCode, tag, hasChildren, attrs);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // tag is not expected to be very large
    if (tag >= UINT32_MAX) {
        LOG_ERROR(sLogger, "Invalid tag value %" PRIu64 " when reading compilation unit data", tag);
        // this is either internal error or data corrupt
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    // we expect to see either DW_TAG_compile_unit or DW_TAG_partial_unit
    // only DW_TAG_compile_unit is supported
    if (tag != DW_TAG_compile_unit) {
        LOG_DEBUG(sLogger, "ERROR: Compilation unit tag %s not supported",
                  getDwarfTagName((unsigned)tag));
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    // read attribute values, according to spec in abbrev
    // we stop when we have name and stmt list
    uint64_t lineSecOffset = 0;
    for (Attr& attr : attrs) {
        if (attr.m_name == DW_AT_name) {
            rc = dwarfReadString(is, attr.m_form, is64Bit, m_dwarfData, cuData.m_fileName);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
        }

        // check for line offset
        else if (attr.m_name == DW_AT_stmt_list) {
            DWARF_READ_OFFSET(is, cuData.m_lineProgOffset, is64Bit);
        }

        // check for low PC
        else if (attr.m_name == DW_AT_low_pc) {
            uint64_t addr = 0;
            DWARF_READ_ADDRESS(is, addr, addressSize);
            if (addr != 0) {
                cuData.m_rangeLow = addr;
            }
        }

        // check for high pc
        else if (attr.m_name == DW_AT_high_pc) {
            // check the form, if it is constant, then we have a size, if it is of address class
            // then we have an address
            if (addressSize == 8) {
                if (attr.m_form == DW_FORM_addr) {
                    DWARF_READ_ADDRESS(is, cuData.m_rangeHigh, addressSize);
                } else if (attr.m_form == DW_FORM_implicit_const) {
                    cuData.m_rangeHigh = cuData.m_rangeLow + attr.m_implicitValue;
                } else {
                    uint64_t rangeSize = 0;
                    DWARF_READ_CONST(is, rangeSize, attr.m_form);
                    cuData.m_rangeHigh = cuData.m_rangeLow + rangeSize;
                }
            }
        }

        // check for CU base address (used by range list)
        else if (attr.m_name == DW_AT_addr_base) {
            // read offset into .debug_addr section
            uint64_t secOffset = 0;
            DWARF_READ_OFFSET(is, secOffset, is64Bit);
            // TODO: is this right? does the first entry contain the base address?
            rc = readAddr(secOffset, cuData.m_baseAddress, addressSize);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
        }

        // check for ranges
        else if (attr.m_name == DW_AT_ranges) {
            uint64_t rngOffset = 0;
            if (attr.m_form == DW_FORM_rnglistx) {
                DWARF_READ_ULEB128(is, rngOffset);
            } else if (attr.m_form == DW_FORM_sec_offset) {
                DWARF_READ_OFFSET(is, rngOffset, is64Bit);
            } else {
                LOG_DEBUG(sLogger, "ERROR: CU Attribute form %s not supported",
                          getDwarfFormName((unsigned)attr.m_form));
                return DBGUTIL_ERR_NOT_IMPLEMENTED;
            }
            // read range list from debug section .debug_rnglists
            rc = readRangeListBounds(rngOffset, cuData.m_baseAddress, is64Bit, addressSize,
                                     cuData.m_rangeLow, cuData.m_rangeHigh);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
        }

        // otherwise skip attribute
        else {
            if (attr.m_form == DW_FORM_string) {
                std::string strVal;
                DBGUTIL_DESERIALIZE_NT_STRING(is, strVal);
            } else if (attr.m_form == DW_FORM_data1) {
                uint8_t val;
                DBGUTIL_DESERIALIZE_INT8(is, val);
            } else if (attr.m_form == DW_FORM_data8) {
                uint64_t val;
                DBGUTIL_DESERIALIZE_INT64(is, val);
            } else if (attr.m_form == DW_FORM_strp) {
                uint64_t strOffset = 0;
                DWARF_READ_OFFSET(is, strOffset, is64Bit);
            } else if (attr.m_form == DW_FORM_line_strp) {
                uint64_t strOffset = 0;
                DWARF_READ_OFFSET(is, strOffset, is64Bit);
            } else if (attr.m_form == DW_FORM_addr) {
                uint64_t addr;
                DWARF_READ_ADDRESS(is, addr, addressSize);
            } else if (attr.m_form == DW_FORM_sec_offset) {
                uint64_t secOffset;
                DWARF_READ_OFFSET(is, secOffset, is64Bit);
            } else {
                LOG_DEBUG(sLogger, "ERROR: CU Attribute form %s not supported",
                          getDwarfFormName((unsigned)attr.m_form));
                return DBGUTIL_ERR_NOT_IMPLEMENTED;
            }
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfUtil::readAbbrevDecl(uint64_t offset, uint64_t abbrevCode, uint64_t& tag,
                                     bool& hasChildren, AttrList& attrs) const {
    FixedInputStream is(m_dwarfData.getDebugAbbrev().m_start + offset,
                        m_dwarfData.getDebugAbbrev().m_size - offset);

    // read abbreviation code, it should be the same as th input
    uint64_t currAbbrevCode = 0;
    do {
        DWARF_READ_ULEB128(is, currAbbrevCode);
        if (currAbbrevCode == 0) {
            // reached end of table
            return DBGUTIL_ERR_NOT_FOUND;
        }

        // NOTE: even if there is no match, we need to continue reading the entire abbrev
        // declaration, because entry size is variable, and we don't have a direct offset

        // read tag
        DWARF_READ_ULEB128(is, tag);

        // read has children (1 byte flag, not LEB128!)
        uint8_t childrenValue = 0;
        DBGUTIL_DESERIALIZE_INT8(is, childrenValue);
        hasChildren = (childrenValue == DW_CHILDREN_yes);

        // read attributes
        bool done = false;
        while (!done) {
            uint64_t name = 0;
            uint64_t form = 0;
            DWARF_READ_ULEB128(is, name);
            DWARF_READ_ULEB128(is, form);
            if (name == 0 && form == 0) {
                // special case: implicit const specifies attr value in abbrev entry
                done = true;
            } else {
                uint64_t val = 0;
                if (form == DW_FORM_implicit_const) {
                    // read one more LEB128 for the attribute value
                    DWARF_READ_ULEB128(is, val);
                }
                // push only when we reach the entry
                if (currAbbrevCode == abbrevCode) {
                    attrs.push_back({name, form, val});
                }
            }
        }
    } while (currAbbrevCode < abbrevCode);

    if (currAbbrevCode == abbrevCode) {
        return DBGUTIL_ERR_OK;
    }
    return DBGUTIL_ERR_NOT_FOUND;
}

DbgUtilErr DwarfUtil::readRangeListBounds(uint64_t rngOffset, uint64_t cuBaseAddr, bool is64Bit,
                                          uint8_t addressSize, uint64_t& rangeLow,
                                          uint64_t& rangeHigh) {
    const DwarfSection& debugSection = m_dwarfData.getDebugRngLists();
    FixedInputStream is(debugSection.m_start + rngOffset, debugSection.m_size - rngOffset);

    // read entries
    bool done = false;
    rangeLow = (uint64_t)-1;
    rangeHigh = (uint64_t)-1;
    do {
        uint64_t startAddress = 0;
        uint64_t endAddress = 0;
        uint8_t kind = 0;
        DBGUTIL_DESERIALIZE_INT8(is, kind);
        if (kind == DW_RLE_end_of_list) {
            done = true;
        } else if (kind == DW_RLE_base_addressx) {
            LOG_DEBUG(sLogger, "ERROR: Range list kind not supported");
            return DBGUTIL_ERR_NOT_IMPLEMENTED;
            /*// read the index operand
            uint64_t index = 0;
            DWARF_READ_ULEB128(is, index);
            // now read the address
            DbgUtilErr rc = readAddr(index, cuBaseAddr, addressSize);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
            continue;*/
        } else if (kind == DW_RLE_startx_endx) {
            LOG_DEBUG(sLogger, "ERROR: Range list kind not supported");
            return DBGUTIL_ERR_NOT_IMPLEMENTED;
            /*// read bounded range indices into .debug_addr
            uint64_t startIndex = 0;
            uint64_t endIndex = 0;
            DWARF_READ_ULEB128(is, startIndex);
            DWARF_READ_ULEB128(is, endIndex);
            // now read bounded range addresses
            DbgUtilErr rc = readAddr(startIndex, startAddress, addressSize);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
            rc = readAddr(endIndex, endAddress, addressSize);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }*/
        } else if (kind == DW_RLE_startx_length) {
            LOG_DEBUG(sLogger, "ERROR: Range list kind not supported");
            return DBGUTIL_ERR_NOT_IMPLEMENTED;
            /*// bounded range with start as index, and then range length
            uint64_t startIndex = 0;
            uint64_t length = 0;
            DWARF_READ_ULEB128(is, startIndex);
            DWARF_READ_ULEB128(is, length);
            // now read bounded range addresses
            DbgUtilErr rc = readAddr(startIndex, startAddress, addressSize);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
            endAddress = startAddress + length;*/
        } else if (kind == DW_RLE_offset_pair) {
            LOG_DEBUG(sLogger, "ERROR: Range list kind not supported");
            return DBGUTIL_ERR_NOT_IMPLEMENTED;
            /*// must be related to a base address, either specifically specified by a prior
            // DW_RLE_base_addressx
            uint64_t startOffset = 0;
            uint64_t endOffset = 0;
            DWARF_READ_ULEB128(is, startOffset);
            DWARF_READ_ULEB128(is, endOffset);
            startAddress = cuBaseAddr + startOffset;
            endAddress = cuBaseAddr + endOffset;*/
        } else if (kind == DW_RLE_base_address) {
            // base address
            DWARF_READ_ADDRESS(is, cuBaseAddr, addressSize);
            continue;
        } else if (kind == DW_RLE_start_end) {
            // read bounded range addresses
            DWARF_READ_ADDRESS(is, startAddress, addressSize);
            DWARF_READ_ADDRESS(is, endAddress, addressSize);
        } else if (kind == DW_RLE_start_length) {
            // bounded range with start address, and then range length
            uint64_t length = 0;
            DWARF_READ_ADDRESS(is, startAddress, addressSize);
            DWARF_READ_ULEB128(is, length);
            endAddress = startAddress + length;
        } else {
            LOG_DEBUG(sLogger, "ERROR: unexpected range list king %u", (unsigned)kind);
            return DBGUTIL_ERR_NOT_IMPLEMENTED;
        }

        // check for end of range set
        if (startAddress == 0 && endAddress == 0) {
            done = true;
        } else {
            LOG_DEBUG(sLogger, "Read range in CU header: %p - %p", (void*)startAddress,
                      (void*)endAddress);
            // skip this weird case (why is this happening?)
            if (startAddress == 0) {
                continue;
            }
            // update total range bound
            if (rangeLow == ((uint64_t)-1) || startAddress < rangeLow) {
                rangeLow = startAddress;
            }
            if (rangeHigh == ((uint64_t)-1) || endAddress > rangeHigh) {
                rangeHigh = endAddress;
            }
        }

    } while (!done);

    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfUtil::readAddr(uint64_t offset, uint64_t& address, uint8_t addressSize) {
    DwarfSection section = {};
    if (m_dwarfData.getSection(".debug_addr", section)) {
        FixedInputStream is(section.m_start + offset, section.m_size - offset);
        DWARF_READ_ADDRESS(is, address, addressSize);
        return DBGUTIL_ERR_OK;
    }
    LOG_DEBUG(sLogger, "ERROR: Section .debug_addr not found");
    return DBGUTIL_ERR_NOT_IMPLEMENTED;
}

DbgUtilErr DwarfUtil::buildRangeCuMap() {
    FixedInputStream is(m_dwarfData.getDebugAddrRanges().m_start,
                        m_dwarfData.getDebugAddrRanges().m_size);

    while (!is.empty()) {
        // read next address range set header
        uint64_t len = 0;
        bool is64Bit = false;
        uint64_t offset = 0;
        uint8_t addressSize = 0;
        DbgUtilErr rc = readAddrRangeHeader(is, len, is64Bit, offset, addressSize);
        if (rc != DBGUTIL_ERR_OK) {
            LOG_DEBUG(sLogger, "ERROR: failed to range range set header: %s", errorCodeToStr(rc));
            return rc;
        }
        LOG_DEBUG(sLogger, "Read address range header: len=%u, CU offset: %u, address-size=%u",
                  (unsigned)len, (unsigned)offset, (unsigned)addressSize);

        // we would like to make sure we don't read past the range set, so we limit the input stream
        uint64_t rawSetSize = len - 8;  // reduce partial header size, do not include length field
        uint64_t isOffset = is.getOffset();
        uint64_t setLimit = isOffset + rawSetSize;
        LOG_DEBUG(sLogger, "Address range len=%u, CU offset=%u, raw-set-size=%u", (unsigned)len,
                  (unsigned)offset, (unsigned)rawSetSize);

        // the entry set must be aligned to a tuple size, which is addr size + range size
        // in case of address size 8 this means 16, and address size 4 requires alignment 8.
        uint32_t align = addressSize * 2;
        uint32_t alignDiff = is.getOffset() % align;
        if (alignDiff != 0) {
            LOG_DEBUG(sLogger, "Set start at offset %u is not aligned to %u, skipping %u bytes",
                      is.getOffset(), align, align - alignDiff);
            size_t bytesSkipped = 0;
            rc = is.skipBytes(align - alignDiff, bytesSkipped);
            if (rc != DBGUTIL_ERR_OK) {
                LOG_DEBUG(sLogger, "ERROR: Failed to skip %u bytes to first range pair: %s",
                          align - alignDiff, errorCodeToStr(rc));
                return rc;
            }
            if (bytesSkipped != (align - alignDiff)) {
                LOG_DEBUG(sLogger,
                          "ERROR: Failed to skip %u bytes to first range pair: end of stream",
                          align - alignDiff);
                return DBGUTIL_ERR_DATA_CORRUPT;
            }
        }
        uint64_t setSize = setLimit - is.getOffset();
        LOG_DEBUG(sLogger, "Address range len=%u, CU offset=%u, set-size=%u", (unsigned)len,
                  (unsigned)offset, (unsigned)setSize);

        // now read address ranges (pairs of address, size, terminated by null pair)
        bool done = false;
        while (!done) {
            uint64_t addr = 0;
            uint64_t size = 0;
            DWARF_READ_ADDRESS(is, addr, addressSize);
            DWARF_READ_ADDRESS(is, size, addressSize);
            if (is.getOffset() > setLimit) {
                LOG_DEBUG(sLogger,
                          "ERROR: range set exceeded limit, no end set zero record pair seen");
                return DBGUTIL_ERR_DATA_CORRUPT;
            }
            if (addr == 0 && size == 0) {
                if (is.getOffset() != setLimit) {
                    LOG_DEBUG(sLogger, "ERROR: range set reached limit but offset is incorrect");
                    return DBGUTIL_ERR_DATA_CORRUPT;
                }
                done = true;
                LOG_DEBUG(sLogger, "End of range set found exactly on correct input stream offset");
            } else {
                if (addr == 0 && size != 0) {
                    // should not happen
                    LOG_DEBUG(sLogger, "WARN: invalid zero based range skipped");
                    // return DBGUTIL_ERR_DATA_CORRUPT;
                    continue;
                }
                if (!m_rangeCuMultiMap[AddrRange(addr, size, offset)].insert(offset).second) {
                    LOG_DEBUG(sLogger, "ERROR: Duplicate offset %u for range %p - %p",
                              (unsigned)offset, (void*)addr, (void*)(addr + size));
                }
            }
        }
    }

    // debug print
    if (canLog(sLogger, LS_DEBUG)) {
        for (const auto& entry : m_rangeCuMultiMap) {
            const AddrRange& range = entry.first;
            LOG_DEBUG(sLogger, "Added range: 0x%p - 0x%p [%u]", (void*)range.m_from,
                      (void*)(range.m_from + range.m_size), (unsigned)range.m_debugInfoOffset);
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfUtil::searchLineProg(const DwarfSearchData& searchData, uint64_t lineProgOffset,
                                     SymbolInfo& symbolInfo) {
    const DwarfSection& debugLineSection = m_dwarfData.getDebugLine();
    FixedInputStream is(debugLineSection.m_start + lineProgOffset,
                        debugLineSection.m_size - lineProgOffset);

    DwarfLineUtil lineUtil;
    return lineUtil.getLineInfo(m_dwarfData, searchData, is, symbolInfo);
}

DbgUtilErr DwarfUtil::searchSymbolInCU(const DwarfSearchData& searchData, uint64_t cuOffset,
                                       SymbolInfo& symbolInfo) {
    CUData cuData;
    DbgUtilErr rc = readCUData(cuOffset, cuData);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    return searchLineProg(searchData, cuData.m_lineProgOffset, symbolInfo);
}

}  // namespace dbgutil
