#ifndef __DWARF_LINE_UTIL_H__
#define __DWARF_LINE_UTIL_H__

#include <vector>

#include "dbgutil_common.h"
#include "dwarf_common.h"
#include "fixed_input_stream.h"
#include "os_symbol_engine.h"

namespace libdbg {

struct DwarfLineStateMachine {
    uint64_t m_address;
    uint32_t m_opIndex;
    uint32_t m_fileIndex;
    uint32_t m_lineNumber;   // 1-based index
    uint32_t m_columnIndex;  // 1-based index
    bool m_isStmt;
    bool m_isBasicBlock;
    bool m_isEndSequence;
    bool m_isPrologueEnd;
    bool m_isEpilogueBegin;
    uint32_t m_isa;
    uint32_t m_discriminator;

    DwarfLineStateMachine()
        : m_address(0),
          m_opIndex(0),
          m_fileIndex(1),
          m_lineNumber(1),
          m_columnIndex(0),
          m_isStmt(false),
          m_isBasicBlock(false),
          m_isEndSequence(false),
          m_isPrologueEnd(false),
          m_isEpilogueBegin(false),
          m_isa(0),
          m_discriminator(0) {}

    inline void reset(bool isStmt) {
        m_address = 0;
        m_opIndex = 0;
        m_fileIndex = 1;
        m_lineNumber = 1;
        m_columnIndex = 0;
        m_isStmt = isStmt;
        m_isBasicBlock = false;
        m_isEndSequence = false;
        m_isPrologueEnd = false;
        m_isEpilogueBegin = false;
        m_isa = 0;
        m_discriminator = 0;
    }

    std::string toString() const;
};

// special opcode - ubyte, no operands
// standard opcodes - ubytes opcode, zero or more operands as LEB128
// extended opcodes - multibyte (page 153)

class DwarfLineUtil {
public:
    DwarfLineUtil();
    ~DwarfLineUtil() {}

    static void initLogger();
    static void termLogger();

    DbgUtilErr getLineInfo(DwarfData& dwarfData, const DwarfSearchData& searchData,
                           FixedInputStream& is, SymbolInfo& symbolInfo);

private:
    struct FileInfo {
        std::string m_name;
        uint32_t m_dirIndex;
        uint64_t m_timestamp;
        uint64_t m_size;
        struct MD5 {
            uint64_t m_lo;
            uint64_t m_hi;

            MD5(uint64_t lo = 0, uint64_t hi = 0) : m_lo(lo), m_hi(hi) {}
        } m_md5;

        FileInfo(const char* name = "", uint32_t dirIndex = 0, uint64_t timestamp = 0,
                 uint64_t size = 0)
            : m_name(name), m_dirIndex(dirIndex), m_timestamp(timestamp), m_size(size) {}
    };

    // important line program header information
    uint8_t m_addressSize;
    uint8_t m_minInstLen;
    uint8_t m_maxOpsPerInst;
    uint8_t m_defaultIsStmt;
    int8_t m_lineBase;
    uint8_t m_lineRange;
    uint8_t m_opCodeBase;  // first special op code
    std::vector<uint8_t> m_stdOpsLen;
    std::vector<std::string> m_dirs;
    std::vector<FileInfo> m_files;
    uint64_t m_startProgramOffset;
    uint64_t m_endProgramOffset;

    struct LineInfo {
        uint64_t m_address;      // relocatable address
        uint32_t m_fileIndex;    // 0-based index to file list
        uint32_t m_lineNumber;   // 1-based index
        uint32_t m_columnIndex;  // 1-based index
        uint32_t m_padding;

        inline bool operator<(const LineInfo& lineInfo) const {
            return m_address < lineInfo.m_address;
        }
    };
    typedef std::vector<LineInfo> LineMatrix;
    LineMatrix m_lineMatrix;

    DwarfLineStateMachine m_stateMachine;

    struct DirEntryFmtDesc {
        uint64_t m_contentType;
        uint64_t m_form;
    };

    DbgUtilErr buildLineMatrix(DwarfData& dwarfData, FixedInputStream& is);

    DbgUtilErr searchLineMatrix(const DwarfSearchData& searchData, SymbolInfo& symbolInfo);

    DbgUtilErr readHeader(FixedInputStream& is, DwarfData& dwarfData);

    DbgUtilErr readFormatList(FixedInputStream& is, std::vector<DirEntryFmtDesc>& entryFmt);

    DbgUtilErr readDirList(FixedInputStream& is, DwarfData& dwarfData, bool is64Bit);

    DbgUtilErr readFileList(FixedInputStream& is, DwarfData& dwarfData, bool is64Bit);

    DbgUtilErr execLineProgram(FixedInputStream& is);

    DbgUtilErr execStandardOpCode(uint8_t opCode, FixedInputStream& is);
    void execSpecialOpCode(uint8_t opCode);
    void advancePC(uint8_t opCode, bool advanceLine = true);
    void advanceAddress(uint64_t opAdvance);
    DbgUtilErr execExtendedOpCode(uint64_t opCode, FixedInputStream& is);
    void appendLineMatrix();
};

}  // namespace libdbg

#endif  // __DWARF_LINE_UTIL_H__