#ifndef __DWARF_COMMON_H__
#define __DWARF_COMMON_H__

#include <unordered_map>

#include "dbgutil_common.h"
#include "input_stream.h"
#include "serializable.h"

namespace libdbg {

struct DwarfSection {
    char* m_start;
    uint64_t m_size;

    DwarfSection(char* start = nullptr, uint64_t size = 0) : m_start(start), m_size(size) {}
};

class DwarfData {
public:
    DwarfData() {}
    DwarfData(const DwarfData&) = default;
    DwarfData(DwarfData&&) = delete;
    DwarfData& operator=(const DwarfData&) = default;
    ~DwarfData() {}

    inline void addSection(const char* name, const DwarfSection& section) {
        m_sectionMap.insert(SectionMap::value_type(name, section));
    }

    bool checkDebugSections();

    inline bool getSection(const char* name, DwarfSection& section) const {
        SectionMap::const_iterator itr = m_sectionMap.find(name);
        if (itr != m_sectionMap.end()) {
            section = itr->second;
            return true;
        }
        return false;
    }

    inline const DwarfSection& getDebugInfo() const { return m_debugInfo; }
    inline const DwarfSection& getDebugAddrRanges() const { return m_debugAddrRanges; }
    inline const DwarfSection& getDebugLine() const { return m_debugLine; }
    inline const DwarfSection& getDebugStr() const { return m_debugStr; }
    inline const DwarfSection& getDebugLineStr() const { return m_debugLineStr; }
    inline const DwarfSection& getDebugAbbrev() const { return m_debugAbbrev; }
    inline const DwarfSection& getDebugRngLists() const { return m_debugRngLists; }
    // inline const DwarfSection& getDebugAddr() const { return m_debugAddr; }

private:
    typedef std::unordered_map<std::string, DwarfSection> SectionMap;
    SectionMap m_sectionMap;

    DwarfSection m_debugInfo;
    DwarfSection m_debugAddrRanges;
    DwarfSection m_debugLine;
    DwarfSection m_debugStr;
    DwarfSection m_debugLineStr;
    DwarfSection m_debugAbbrev;
    DwarfSection m_debugRngLists;
    // DwarfSection m_debugAddr;
};

struct DwarfSearchData {
    void* m_symbolAddress;
    void* m_moduleBaseAddress;
    uint64_t m_symbolOffset;
    void* m_relocationBase;
    uint64_t m_relocatedAddress;
};

extern DbgUtilErr dwarfReadInitialLength(InputStream& is, uint64_t& len, bool& is64Bit);
extern DbgUtilErr dwarfReadOffset(InputStream& is, uint64_t& offset, bool is64Bit);
extern DbgUtilErr dwarfReadAddress(InputStream& is, uint64_t& offset, uint64_t addressSize);
extern DbgUtilErr dwarfReadULEB128(InputStream& is, uint64_t& result);
extern DbgUtilErr dwarfReadSLEB128(InputStream& is, int64_t& result);
extern DbgUtilErr dwarfReadString(InputStream& is, uint64_t form, bool is64Bit,
                                  DwarfData& dwarfData, std::string& result);

#define DWARF_READ_INIT_LEN(is, len, is64Bit)                          \
    {                                                                  \
        DbgUtilErr rcLocal = dwarfReadInitialLength(is, len, is64Bit); \
        if (rcLocal != DBGUTIL_ERR_OK) {                               \
            return rcLocal;                                            \
        }                                                              \
    }

#define DWARF_READ_OFFSET(is, offset, is64Bit)                     \
    {                                                              \
        DbgUtilErr rcLocal = dwarfReadOffset(is, offset, is64Bit); \
        if (rcLocal != DBGUTIL_ERR_OK) {                           \
            return rcLocal;                                        \
        }                                                          \
    }

#define DWARF_READ_ADDRESS(is, offset, addressSize)                     \
    {                                                                   \
        DbgUtilErr rcLocal = dwarfReadAddress(is, offset, addressSize); \
        if (rcLocal != DBGUTIL_ERR_OK) {                                \
            return rcLocal;                                             \
        }                                                               \
    }

#define DWARF_READ_ULEB128(is, value)                     \
    {                                                     \
        DbgUtilErr rcLocal = dwarfReadULEB128(is, value); \
        if (rcLocal != DBGUTIL_ERR_OK) {                  \
            return rcLocal;                               \
        }                                                 \
    }

#define DWARF_READ_SLEB128(is, value)                     \
    {                                                     \
        DbgUtilErr rcLocal = dwarfReadSLEB128(is, value); \
        if (rcLocal != DBGUTIL_ERR_OK) {                  \
            return rcLocal;                               \
        }                                                 \
    }

#define DWARF_READ_CONST1(is, value)          \
    {                                         \
        uint8_t value8 = 0;                   \
        DBGUTIL_DESERIALIZE_INT8(is, value8); \
        value = value8;                       \
    }

#define DWARF_READ_CONST2(is, value)            \
    {                                           \
        uint16_t value16 = 0;                   \
        DBGUTIL_DESERIALIZE_INT16(is, value16); \
        value = value16;                        \
    }

#define DWARF_READ_CONST4(is, value)            \
    {                                           \
        uint32_t value32 = 0;                   \
        DBGUTIL_DESERIALIZE_INT32(is, value32); \
        value = value32;                        \
    }

#define DWARF_READ_CONST8(is, value)            \
    {                                           \
        uint64_t value64 = 0;                   \
        DBGUTIL_DESERIALIZE_INT64(is, value64); \
        value = value64;                        \
    }

#define DWARF_READ_CONST(is, value, form)   \
    if (form == DW_FORM_data1) {            \
        DWARF_READ_CONST1(is, value);       \
    } else if (form == DW_FORM_data2) {     \
        DWARF_READ_CONST2(is, value);       \
    } else if (form == DW_FORM_data4) {     \
        DWARF_READ_CONST4(is, value);       \
    } else if (form == DW_FORM_data8) {     \
        DWARF_READ_CONST8(is, value);       \
    } else if (form == DW_FORM_udata) {     \
        DWARF_READ_ULEB128(is, value);      \
    } else {                                \
        return DBGUTIL_ERR_NOT_IMPLEMENTED; \
    }

}  // namespace libdbg

#endif  // __DWARF_COMMON_H__