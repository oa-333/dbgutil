#ifndef __DWARF_UTIL_H__
#define __DWARF_UTIL_H__

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dbgutil_common.h"
#include "dwarf_common.h"
#include "input_stream.h"
#include "os_symbol_engine.h"

namespace dbgutil {

class DwarfUtil {
public:
    DwarfUtil();

    static void initLogger();
    static void termLogger();

    DbgUtilErr open(const DwarfData& dwarfData, void* moduleBase, bool is664Bit, bool isExe);

    DbgUtilErr searchSymbol(void* symAddress, SymbolInfo& symbolInfo,
                            void* relocationBase = nullptr);

private:
    DwarfData m_dwarfData;
    void* m_moduleBase;
    bool m_isExe;
    bool m_is64Bit;

    struct AddrRange {
        uint64_t m_from;
        uint64_t m_size;
        uint64_t m_debugInfoOffset;

        inline bool operator==(const AddrRange& addrRange) const {
            return m_from == addrRange.m_from && m_size == addrRange.m_size;
        }

        inline bool operator<(const AddrRange& addrRange) const {
            return m_from < addrRange.m_from;
        }

        inline bool operator<(uint64_t symOffset) const { return m_from + m_size <= symOffset; }

        inline bool contains(uint64_t offset) const {
            return offset >= m_from && offset < m_from + m_size;
        }
    };

    typedef std::unordered_set<uint64_t> OffsetSet;
    // NOTE: using transparent comparator template version of map.lower_bound
    typedef std::map<AddrRange, OffsetSet, std::less<>> RangeCuMultiMap;
    RangeCuMultiMap m_rangeCuMultiMap;

    struct CUData {
        std::string m_fileName;
        uint64_t m_lineProgOffset;
        uint64_t m_baseAddress;
        uint64_t m_rangeLow;
        uint64_t m_rangeHigh;

        CUData() : m_lineProgOffset(0), m_baseAddress(0), m_rangeLow(0), m_rangeHigh(0) {}

        inline bool operator<(const CUData& cuData) const { return m_rangeLow < cuData.m_rangeLow; }

        inline bool contains(uint64_t offset) const {
            return offset >= m_rangeLow && offset < m_rangeHigh;
        }
    };

    DbgUtilErr readCUData(uint64_t offset, CUData& cuData);
    DbgUtilErr buildRangeCuMap();

    DbgUtilErr searchLineProg(const DwarfSearchData& searchData, uint64_t lineProgOffset,
                              SymbolInfo& symbolInfo);
    DbgUtilErr searchSymbolInCU(const DwarfSearchData& searchData, uint64_t cuOffset,
                                SymbolInfo& symbolInfo);

    struct Attr {
        uint64_t m_name;
        uint64_t m_form;
        uint64_t m_implicitValue;
    };
    typedef std::vector<Attr> AttrList;

    DbgUtilErr readAddrRangeHeader(InputStream& is, uint64_t& len, bool& is64Bit, uint64_t& offset,
                                   uint8_t& addressSize);
    DbgUtilErr readCUHeader(InputStream& is, uint64_t& len, uint64_t& abbrevOffset,
                            uint8_t& addressSize, bool& is64Bit);
    DbgUtilErr readAbbrevDecl(uint64_t offset, uint64_t abbrevCode, uint64_t& tag,
                              bool& hasChildren, AttrList& attrs) const;
    DbgUtilErr readRangeListBounds(uint64_t rngOffset, uint64_t cuBaseAddr, bool is64Bit,
                                   uint8_t addressSize, uint64_t& rangeLow, uint64_t& rangeHigh);
    DbgUtilErr readAddr(uint64_t offset, uint64_t& address, uint8_t addressSize);
};

}  // namespace dbgutil

#endif  // __DWARF_UTIL_H__