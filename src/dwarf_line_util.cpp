#include "dwarf_line_util.h"

#include <algorithm>
#include <cinttypes>
#include <climits>
#include <iomanip>
#include <sstream>

#include "dbgutil_common.h"
#include "dbgutil_log_imp.h"
#include "dwarf_def.h"

namespace dbgutil {

static Logger sLogger;

void DwarfLineUtil::initLogger() { registerLogger(sLogger, "dwarf_line_util"); }
void DwarfLineUtil::termLogger() { unregisterLogger(sLogger); }

std::string DwarfLineStateMachine::toString() const {
    std::stringstream s;
    s << "StateMachine = {address: 0x" << std::hex << m_address << std::dec
      << ", line: " << m_lineNumber << ", file: " << m_fileIndex << "}";
    return s.str();
}

DwarfLineUtil::DwarfLineUtil() {}

DbgUtilErr DwarfLineUtil::getLineInfo(DwarfData& dwarfData, const DwarfSearchData& searchData,
                                      FixedInputStream& is, SymbolInfo& symbolInfo) {
    // build line matrix on-demand
    if (m_lineMatrix.empty()) {
        DbgUtilErr rc = buildLineMatrix(dwarfData, is);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
    }

    // now search in line matrix
    return searchLineMatrix(searchData, symbolInfo);
}

DbgUtilErr DwarfLineUtil::buildLineMatrix(DwarfData& dwarfData, FixedInputStream& is) {
    DbgUtilErr rc = readHeader(is, dwarfData);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    m_stateMachine.reset(m_defaultIsStmt ? true : false);
    return execLineProgram(is);
}

DbgUtilErr DwarfLineUtil::searchLineMatrix(const DwarfSearchData& searchData,
                                           SymbolInfo& symbolInfo) {
    // search relocated address (translated to debug info base) line in matrix
    // NOTE: we search for the first entry whose address >= search address, special care required,
    // see below for more explanation
    LOG_DEBUG(sLogger, "Searching for relocated address %p", (void*)searchData.m_relocatedAddress)
    LineMatrix::iterator itr =
        std::lower_bound(m_lineMatrix.begin(), m_lineMatrix.end(), searchData.m_relocatedAddress,
                         [](const LineInfo& lineInfo, uint64_t relocSymAddr) {
                             return lineInfo.m_address < relocSymAddr;
                         });

    // NOTE: we get the first iterator for which the predicate is false, or end(),
    // so end() means all addresses are less than searched address.
    if (itr == m_lineMatrix.end()) {
        return DBGUTIL_ERR_NOT_FOUND;
    }

    // NOTE: since we passed strict less operator "<", then if the last address equals the searched
    // address, then we would get a valid iterator pointing to the last valid element (not end()).
    // if we get the begin() iterator it means that either the address equals the first element (so
    // strict "<" return false), or that the searched address is lower than the first address.
    // in simpler words: first check that the current iterator points to an address equal to the
    // searched address, and if so return it, otherwise check for the previous iterator (unless this
    // is begin iterator), which will definitely return true for "<".
    bool found = false;
    if (itr->m_address == searchData.m_relocatedAddress) {
        found = true;
    } else if (itr != m_lineMatrix.begin()) {
        --itr;
        if (itr->m_address <= searchData.m_relocatedAddress) {
            found = true;
        }
    }

    if (found) {
        // at this point it is possible to have several entries with the searched address, so we
        // prefer to choose the one which has the main file, rather that STL or libstdc stuff
        LineMatrix::iterator itr2 = itr;
        while (itr2 != m_lineMatrix.end() && itr2->m_address == itr->m_address &&
               m_files[itr->m_fileIndex].m_name.compare(m_files[0].m_name) != 0) {
            ++itr2;
        }
        if (itr2 != m_lineMatrix.end()) {
            // we found an entry with the same address but pointing to main file of CU, so take it
            itr = itr2;
        }

        LineInfo& lineInfo = *itr;
        FileInfo& fileInfo = m_files[lineInfo.m_fileIndex];
        // NOTE: symbol start address is extracted from symbol table
        std::string dir = m_dirs[fileInfo.m_dirIndex];
        symbolInfo.m_fileName = dir + "/" + fileInfo.m_name;
        symbolInfo.m_lineNumber = lineInfo.m_lineNumber;
        symbolInfo.m_columnIndex = lineInfo.m_columnIndex;
        LOG_DEBUG(sLogger, "Relocated address %p found at %p, file %s, line %u",
                  (void*)searchData.m_relocatedAddress, (void*)lineInfo.m_address,
                  symbolInfo.m_fileName.c_str(), symbolInfo.m_lineNumber);
        return DBGUTIL_ERR_OK;
    }

    return DBGUTIL_ERR_NOT_FOUND;
}

DbgUtilErr DwarfLineUtil::readHeader(FixedInputStream& is, DwarfData& dwarfData) {
    uint64_t len = 0;
    bool is64Bit = false;
    DWARF_READ_INIT_LEN(is, len, is64Bit);
    m_endProgramOffset = is.getOffset() + len;

    uint16_t version = 0;
    DBGUTIL_DESERIALIZE_INT16(is, version);
    if (version != 5) {
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    DBGUTIL_DESERIALIZE_INT8(is, m_addressSize);

    uint8_t segmentSelector = 0;
    DBGUTIL_DESERIALIZE_INT8(is, segmentSelector);

    uint64_t headerLength = 0;
    DWARF_READ_OFFSET(is, headerLength, is64Bit);
    m_startProgramOffset = is.getOffset() + headerLength;

    DBGUTIL_DESERIALIZE_INT8(is, m_minInstLen);
    DBGUTIL_DESERIALIZE_INT8(is, m_maxOpsPerInst);
    DBGUTIL_DESERIALIZE_INT8(is, m_defaultIsStmt);
    DBGUTIL_DESERIALIZE_INT8(is, m_lineBase);
    DBGUTIL_DESERIALIZE_INT8(is, m_lineRange);
    DBGUTIL_DESERIALIZE_INT8(is, m_opCodeBase);

    // standard op code operand count array
    // NOTE: no opcode zero, and the array is offset by 1 (i.e. index 0 is for opcode 1)
    for (uint8_t i = 1; i < m_opCodeBase; ++i) {
        uint8_t opCount = 0;
        DBGUTIL_DESERIALIZE_INT8(is, opCount);
        m_stdOpsLen.push_back(opCount);
    }

    // read dir array
    DbgUtilErr rc = readDirList(is, dwarfData, is64Bit);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    rc = readFileList(is, dwarfData, is64Bit);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfLineUtil::readFormatList(FixedInputStream& is,
                                         std::vector<DirEntryFmtDesc>& entryFmt) {
    uint8_t fmtCount = 0;
    DBGUTIL_DESERIALIZE_INT8(is, fmtCount);
    for (uint8_t i = 0; i < fmtCount; ++i) {
        uint64_t ctCode = 0;
        uint64_t form = 0;
        DWARF_READ_ULEB128(is, ctCode);
        DWARF_READ_ULEB128(is, form);
        entryFmt.push_back({ctCode, form});
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfLineUtil::readDirList(FixedInputStream& is, DwarfData& dwarfData, bool is64Bit) {
    // read dir/file entry format array
    // each dir/file entry below has all the format entries repeated in each dir/file entry
    std::vector<DirEntryFmtDesc> entryFmt;
    DbgUtilErr rc = readFormatList(is, entryFmt);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // dir name array
    uint64_t dirCount = 0;
    DWARF_READ_ULEB128(is, dirCount);
    for (uint64_t i = 0; i < dirCount; ++i) {
        for (uint8_t j = 0; j < entryFmt.size(); ++j) {
            uint64_t ctCode = entryFmt[j].m_contentType;
            uint64_t form = entryFmt[j].m_form;
            if (ctCode == DW_LNCT_path) {
                std::string name;
                rc = dwarfReadString(is, form, is64Bit, dwarfData, name);
                if (rc != DBGUTIL_ERR_OK) {
                    return rc;
                }
                m_dirs.push_back(name);
                LOG_DEBUG(sLogger, "Read line program dir: %s", name.c_str());
            } else {
                return DBGUTIL_ERR_NOT_IMPLEMENTED;
            }
        }
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfLineUtil::readFileList(FixedInputStream& is, DwarfData& dwarfData, bool is64Bit) {
    // read dir/file entry format array
    // each dir/file entry below has all the format entries repeated in each dir/file entry
    std::vector<DirEntryFmtDesc> entryFmt;
    DbgUtilErr rc = readFormatList(is, entryFmt);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // file name array
    uint64_t fileCount = 0;
    DWARF_READ_ULEB128(is, fileCount);
    for (uint64_t i = 0; i < fileCount; ++i) {
        for (uint8_t j = 0; j < entryFmt.size(); ++j) {
            uint64_t ctCode = entryFmt[j].m_contentType;
            uint64_t form = entryFmt[j].m_form;
            if (ctCode == DW_LNCT_path) {
                std::string name;
                rc = dwarfReadString(is, form, is64Bit, dwarfData, name);
                if (rc != DBGUTIL_ERR_OK) {
                    return rc;
                }
                if (!m_files.empty()) {
                    LOG_DEBUG(sLogger, "Read line program file: %s/%s",
                              m_dirs[m_files.back().m_dirIndex].c_str(),
                              m_files.back().m_name.c_str());
                }
                m_files.push_back({name.c_str()});
            } else if (ctCode == DW_LNCT_directory_index) {
                uint64_t index = 0;
                DWARF_READ_CONST(is, index, form);
                if (m_files.empty()) {
                    return DBGUTIL_ERR_DATA_CORRUPT;
                }
                // directory index is never excepted to be very large
                if (index >= UINT32_MAX) {
                    LOG_ERROR(sLogger, "Invalid directory index %" PRIu64 " in line program",
                              index);
                    // this means either internal error or data corrupt
                    return DBGUTIL_ERR_DATA_CORRUPT;
                }
                m_files.back().m_dirIndex = (uint32_t)index;
            } else if (ctCode == DW_LNCT_timestamp) {
                if (form == DW_FORM_block) {
                    return DBGUTIL_ERR_NOT_IMPLEMENTED;
                }
                uint64_t timestamp = 0;
                DWARF_READ_CONST(is, timestamp, form);
                if (m_files.empty()) {
                    return DBGUTIL_ERR_DATA_CORRUPT;
                }
                m_files.back().m_timestamp = timestamp;
            } else if (ctCode == DW_LNCT_size) {
                uint64_t size = 0;
                DWARF_READ_CONST(is, size, form);
                if (m_files.empty()) {
                    return DBGUTIL_ERR_DATA_CORRUPT;
                }
                m_files.back().m_size = size;
            } else if (ctCode == DW_LNCT_MD5) {
                uint64_t lo = 0;
                uint64_t hi = 0;
                DBGUTIL_DESERIALIZE_INT64(is, lo);
                DBGUTIL_DESERIALIZE_INT64(is, hi);
                if (m_files.empty()) {
                    return DBGUTIL_ERR_DATA_CORRUPT;
                }
                m_files.back().m_md5.m_lo = lo;
                m_files.back().m_md5.m_hi = hi;
            } else {
                // MD5 not supported for now
                return DBGUTIL_ERR_NOT_IMPLEMENTED;
            }
        }
    }

    if (!m_files.empty()) {
        LOG_DEBUG(sLogger, "Read line program file: %s/%s",
                  m_dirs[m_files.back().m_dirIndex].c_str(), m_files.back().m_name.c_str());
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr DwarfLineUtil::execLineProgram(FixedInputStream& is) {
    // skip bytes according to header length
    uint64_t offset = is.getOffset();
    if (offset > m_startProgramOffset) {
        // exceeded expected start of program line offset
        return DBGUTIL_ERR_INTERNAL_ERROR;
    }
    if (offset < m_startProgramOffset) {
        size_t bytesSkiped = 0;
        DbgUtilErr rc = is.skipBytes(m_startProgramOffset - offset, bytesSkiped);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        if (bytesSkiped != (m_startProgramOffset - offset)) {
            return DBGUTIL_ERR_INTERNAL_ERROR;
        }
    }

    // read as many bytes as in unit length
    while (is.getOffset() < m_endProgramOffset) {
        // read instruction op-code byte
        uint8_t opCode = 0;
        DBGUTIL_DESERIALIZE_INT8(is, opCode);
        if (opCode == 0) {
            // extended op-code
            uint64_t instSizeBytes = 0;
            DWARF_READ_ULEB128(is, instSizeBytes);
            DBGUTIL_DESERIALIZE_INT8(is, opCode);
            DbgUtilErr rc = execExtendedOpCode(opCode, is);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
        } else if (opCode < m_opCodeBase) {
            // standard op-code
            DbgUtilErr rc = execStandardOpCode(opCode, is);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
        } else {
            // special op-code
            execSpecialOpCode(opCode);
        }
        LOG_DEBUG(sLogger, "%s", m_stateMachine.toString().c_str());
    }

    // sort matrix for quick search
    // NOTE: since a single address may be associated with several files/lines (especially in
    // release builds), we would like to preserve original order as it appears in the line program,
    // therefore we use here stable_sort(), rather than sort().
    std::stable_sort(m_lineMatrix.begin(), m_lineMatrix.end());

    // print matrix
    if (canLog(sLogger, LS_DEBUG)) {
        LOG_DEBUG(sLogger, "ADDR   LINE FILE");
        for (uint32_t i = 0; i < m_lineMatrix.size(); ++i) {
            LineInfo& li = m_lineMatrix[i];
            const char* fileName =
                li.m_fileIndex < m_files.size() ? m_files[li.m_fileIndex].m_name.c_str() : "";
            LOG_DEBUG(sLogger, "%0.6p %0.4u %u --> %s", (void*)li.m_address,
                      (unsigned)li.m_lineNumber, (unsigned)li.m_fileIndex, fileName);
        }
    }

    return DBGUTIL_ERR_OK;
}

void DwarfLineUtil::appendLineMatrix() {
    m_lineMatrix.push_back({m_stateMachine.m_address, m_stateMachine.m_fileIndex,
                            m_stateMachine.m_lineNumber, m_stateMachine.m_columnIndex});
}

DbgUtilErr DwarfLineUtil::execStandardOpCode(uint8_t opCode, FixedInputStream& is) {
    switch (opCode) {
        case DW_LNS_copy:
            // copy row to matrix
            appendLineMatrix();
            m_stateMachine.m_discriminator = 0;
            m_stateMachine.m_isBasicBlock = false;
            m_stateMachine.m_isPrologueEnd = false;
            m_stateMachine.m_isEpilogueBegin = false;
            LOG_DEBUG(sLogger, "Executing DW_LNS_copy");
            break;

        case DW_LNS_advance_pc: {
            uint64_t opAdvance = 0;
            DWARF_READ_ULEB128(is, opAdvance);
            advanceAddress(opAdvance);
            LOG_DEBUG(sLogger, "Executed DW_LNS_advance_pc: %" PRIu64 " --> %s", opAdvance,
                      m_stateMachine.toString().c_str());
            break;
        }

        case DW_LNS_advance_line: {
            int64_t advance = 0;
            DWARF_READ_SLEB128(is, advance);
            // the advance is never expected to be too large, so we check for bounds
            if (advance >= INT32_MAX || advance <= INT32_MIN) {
                LOG_ERROR(sLogger, "Invalid line advance value %" PRId64 " in line program",
                          advance);
                // this means either internal error or corrupt data
                return DBGUTIL_ERR_DATA_CORRUPT;
            }
            // carefully update the value, line number cannot go below 1
            if (advance >= 0) {
                m_stateMachine.m_lineNumber += (int32_t)advance;
            } else {
                uint32_t advanceAbs32 = (uint32_t)(-advance);
                if (advanceAbs32 >= m_stateMachine.m_lineNumber) {
                    LOG_ERROR(sLogger,
                              "Line advance value %" PRId64
                              " will cause line number %u to reach invalid value",
                              advance, m_stateMachine.m_lineNumber);
                    // this is either internal error or corrupt data
                    return DBGUTIL_ERR_DATA_CORRUPT;
                }
                m_stateMachine.m_lineNumber += (int32_t)advance;
            }
            LOG_DEBUG(sLogger, "Executed DW_LNS_advance_line: %" PRId64 " --> %s", advance,
                      m_stateMachine.toString().c_str());
            break;
        }

        case DW_LNS_set_file: {
            uint64_t fileIndex = 0;
            DWARF_READ_ULEB128(is, fileIndex);
            LOG_DEBUG(sLogger, "Executed DW_LNS_set_file: %" PRIu64 " --> %s", fileIndex,
                      m_stateMachine.toString().c_str());
            // file index is never expected to be too large
            if (fileIndex >= UINT32_MAX) {
                LOG_ERROR(sLogger, "Invalid file index %" PRIu64 " in line program", fileIndex);
                // this is either internal error or corrupt data
                return DBGUTIL_ERR_DATA_CORRUPT;
            }
            m_stateMachine.m_fileIndex = (uint32_t)fileIndex;
            break;
        }

        case DW_LNS_set_column: {
            uint64_t columnIndex = 0;
            DWARF_READ_ULEB128(is, columnIndex);
            // file index is never expected to be too large
            if (columnIndex >= UINT32_MAX) {
                LOG_ERROR(sLogger, "Invalid column index %" PRIu64 " in line program", columnIndex);
                // this is either internal error or corrupt data
                return DBGUTIL_ERR_DATA_CORRUPT;
            }
            m_stateMachine.m_columnIndex = (uint32_t)columnIndex;
            LOG_DEBUG(sLogger, "Executed DW_LNS_set_column: %" PRIu64, columnIndex);
            break;
        }

        case DW_LNS_negate_stmt:
            m_stateMachine.m_isStmt = !m_stateMachine.m_isStmt;
            LOG_DEBUG(sLogger, "Executed DW_LNS_negate_stmt");
            break;

        case DW_LNS_set_basic_block:
            m_stateMachine.m_isBasicBlock = true;
            LOG_DEBUG(sLogger, "Executed DW_LNS_set_basic_block");
            break;

        case DW_LNS_const_add_pc:
            // this does not affect line number, only address and op index
            advancePC(255, false);
            LOG_DEBUG(sLogger, "Executed DW_LNS_const_add_pc --> %s",
                      m_stateMachine.toString().c_str());
            break;

        case DW_LNS_fixed_advance_pc: {
            uint16_t advance = 0;
            DBGUTIL_DESERIALIZE_INT16(is, advance);
            m_stateMachine.m_address += advance;
            m_stateMachine.m_opIndex = 0;
            LOG_DEBUG(sLogger, "Executed DW_LNS_fixed_advance_pc: %u --> %s", (unsigned)advance,
                      m_stateMachine.toString().c_str());
            break;
        }

        case DW_LNS_set_prologue_end:
            m_stateMachine.m_isPrologueEnd = true;
            LOG_DEBUG(sLogger, "Executed DW_LNS_set_prologue_end");
            break;

        case DW_LNS_set_epilogue_begin:
            m_stateMachine.m_isEpilogueBegin = true;
            LOG_DEBUG(sLogger, "Executed DW_LNS_set_epilogue_begin");
            break;

        case DW_LNS_set_isa: {
            uint64_t value = 0;
            DWARF_READ_ULEB128(is, value);
            if (value > UINT_MAX) {
                return DBGUTIL_ERR_INTERNAL_ERROR;
            }
            // not sure what range of values is expected here, for now we restrict to uint32_t
            if (value >= UINT32_MAX) {
                LOG_ERROR(sLogger, "Invalid isa value %" PRIu64 " in line program", value);
                // this is either internal error or corrupt data
                return DBGUTIL_ERR_DATA_CORRUPT;
            }
            m_stateMachine.m_isa = (uint32_t)value;
            LOG_DEBUG(sLogger, "Executed DW_LNS_set_isa: %" PRIu64, value);
            break;
        }

        default:
            return DBGUTIL_ERR_INTERNAL_ERROR;
    }

    return DBGUTIL_ERR_OK;
}

void DwarfLineUtil::execSpecialOpCode(uint8_t opCode) {
    advancePC(opCode, true);
    LOG_DEBUG(sLogger, "Executed special opcode: %u --> %s", (unsigned)opCode,
              m_stateMachine.toString().c_str());
    appendLineMatrix();

    m_stateMachine.m_isBasicBlock = false;
    m_stateMachine.m_isPrologueEnd = false;
    m_stateMachine.m_isEpilogueBegin = false;
    m_stateMachine.m_discriminator = 0;
}

void DwarfLineUtil::advancePC(uint8_t opCode, bool advanceLine /* = false */) {
    if (opCode < m_opCodeBase) {
        LOG_DEBUG(sLogger, "ERROR: Invalid op-code %u, smaller than op-code-base %u",
                  (unsigned)opCode, (unsigned)m_opCodeBase);
        return;
    }
    uint8_t adjustedOpCode = (uint8_t)(opCode - m_opCodeBase);
    uint8_t opAdvance = (uint8_t)(adjustedOpCode / m_lineRange);

    advanceAddress(opAdvance);
    if (advanceLine) {
        int8_t lineAdvance = m_lineBase + (adjustedOpCode % m_lineRange);
        m_stateMachine.m_lineNumber += lineAdvance;
    }
}

void DwarfLineUtil::advanceAddress(uint64_t opAdvance) {
    // NOTE: when max-ops-per-instruction is 1 then op-index is always zero
    // as a result, the address advance formula collapses to:
    //
    // address += min-instruction-len * op-advance
    //
    // when min-instruction-len is also 1, then this collapses to the simple expression:
    //
    // address += op-advance
    //
    // so in this case, DW_LNS_advance_pc simply advances the address by as many bytes as given by
    // the op-advance argument
    m_stateMachine.m_address +=
        m_minInstLen * (m_stateMachine.m_opIndex + opAdvance) / m_maxOpsPerInst;
    m_stateMachine.m_opIndex = (m_stateMachine.m_opIndex + opAdvance) % m_maxOpsPerInst;
}

DbgUtilErr DwarfLineUtil::execExtendedOpCode(uint64_t opCode, FixedInputStream& is) {
    switch (opCode) {
        case DW_LNE_end_sequence:
            m_stateMachine.m_isEndSequence = true;
            LOG_DEBUG(sLogger, "Executed DW_LNE_end_sequence");
            // append row to matrix
            appendLineMatrix();
            m_stateMachine.reset(m_defaultIsStmt ? true : false);
            break;

        case DW_LNE_set_address: {
            // NOTE: address is relocatable, should check this carefully
            uint64_t address = 0;
            DWARF_READ_ADDRESS(is, address, m_addressSize);
            m_stateMachine.m_address = address;
            m_stateMachine.m_opIndex = 0;
            LOG_DEBUG(sLogger, "Executed DW_LNE_set_address: %p --> %s", (void*)address,
                      m_stateMachine.toString().c_str());
            break;
        }

        case DW_LNE_set_discriminator: {
            uint64_t value = 0;
            DWARF_READ_ULEB128(is, value);
            // not sure what range of values is expected here, for now we restrict to uint32_t
            if (value >= UINT32_MAX) {
                LOG_ERROR(sLogger, "Invalid discriminator value %" PRIu64 " in line program",
                          value);
                // this is either internal error or corrupt data
                return DBGUTIL_ERR_DATA_CORRUPT;
            }
            m_stateMachine.m_discriminator = (uint32_t)value;
            LOG_DEBUG(sLogger, "Executed DW_LNE_set_discriminator: %" PRIu64, value);
            break;
        }

        default:
            return DBGUTIL_ERR_INTERNAL_ERROR;
    }

    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil
