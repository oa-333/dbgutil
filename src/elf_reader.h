#ifndef __ELF_UTIL_H__
#define __ELF_UTIL_H__

#include "dbg_util_def.h"

#ifdef DBGUTIL_LINUX

#include <elf.h>

#include <unordered_map>
#include <vector>

#include "dbgutil_common.h"
#include "os_image_reader.h"

namespace dbgutil {

class ElfReader : public OsImageReader {
public:
    ElfReader();
    ~ElfReader() final {}

protected:
    DbgUtilErr readImage() final;
    void resetData() final;

private:
    union {
        Elf32_Ehdr m_hdr32;
        Elf64_Ehdr m_hdr64;
    } m_hdr;
    std::vector<char> m_shStrTab;
    std::vector<char> m_strTab;
    std::vector<char> m_symTab;
    uint64_t m_symTabSize;
    uint64_t m_symEntrySize;
    elog::ELogLogger* m_logger;

    DbgUtilErr verifyHeader();
    DbgUtilErr readElf();

    DbgUtilErr checkHeader();
    DbgUtilErr checkHeader32(Elf32_Ehdr* hdr);
    DbgUtilErr checkHeader64(Elf64_Ehdr* hdr);

    DbgUtilErr buildSectionMap();
    DbgUtilErr buildSectionMap32(Elf32_Ehdr* hdr);
    DbgUtilErr buildSectionMap64(Elf64_Ehdr* hdr);

    DbgUtilErr getSectionHeaderStrTab();
    DbgUtilErr getSectionHeaderStrTab32(Elf32_Ehdr* hdr);
    DbgUtilErr getSectionHeaderStrTab64(Elf64_Ehdr* hdr);

    DbgUtilErr getStrTab();
    DbgUtilErr getStrTab32(Elf32_Ehdr* hdr);
    DbgUtilErr getStrTab64(Elf64_Ehdr* hdr);

    DbgUtilErr getSymTab();
    DbgUtilErr getSymTab32(Elf32_Ehdr* hdr);
    DbgUtilErr getSymTab64(Elf64_Ehdr* hdr);

    DbgUtilErr buildSymInfoSet();
    DbgUtilErr buildSymInfoSet32(Elf32_Ehdr* hdr);
    DbgUtilErr buildSymInfoSet64(Elf64_Ehdr* hdr);

    void dumpSectionHeaders();
    void dumpSectionHeaders32();
    void dumpSectionHeaders64();
};

extern DbgUtilErr initElfReader();
extern DbgUtilErr termElfReader();

}  // namespace dbgutil

#endif  // DBGUTIL_LINUX

#endif  // __ELF_UTIL_H__