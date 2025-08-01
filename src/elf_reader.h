#ifndef __ELF_UTIL_H__
#define __ELF_UTIL_H__

#include "libdbg_def.h"

#ifdef DBGUTIL_LINUX

#include <elf.h>

#include <unordered_map>
#include <vector>

#include "dbgutil_common.h"
#include "os_image_reader.h"

namespace libdbg {

class ElfReader : public OsImageReader {
public:
    ElfReader();
    ~ElfReader() final {}

protected:
    LibDbgErr readImage() final;
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

    LibDbgErr verifyHeader();
    LibDbgErr readElf();

    LibDbgErr checkHeader();
    LibDbgErr checkHeader32(Elf32_Ehdr* hdr);
    LibDbgErr checkHeader64(Elf64_Ehdr* hdr);

    LibDbgErr buildSectionMap();
    LibDbgErr buildSectionMap32(Elf32_Ehdr* hdr);
    LibDbgErr buildSectionMap64(Elf64_Ehdr* hdr);

    LibDbgErr getSectionHeaderStrTab();
    LibDbgErr getSectionHeaderStrTab32(Elf32_Ehdr* hdr);
    LibDbgErr getSectionHeaderStrTab64(Elf64_Ehdr* hdr);

    LibDbgErr getStrTab();
    LibDbgErr getStrTab32(Elf32_Ehdr* hdr);
    LibDbgErr getStrTab64(Elf64_Ehdr* hdr);

    LibDbgErr getSymTab();
    LibDbgErr getSymTab32(Elf32_Ehdr* hdr);
    LibDbgErr getSymTab64(Elf64_Ehdr* hdr);

    LibDbgErr buildSymInfoSet();
    LibDbgErr buildSymInfoSet32(Elf32_Ehdr* hdr);
    LibDbgErr buildSymInfoSet64(Elf64_Ehdr* hdr);

    void dumpSectionHeaders();
    void dumpSectionHeaders32();
    void dumpSectionHeaders64();
};

extern LibDbgErr initElfReader();
extern LibDbgErr termElfReader();

}  // namespace libdbg

#endif  // DBGUTIL_LINUX

#endif  // __ELF_UTIL_H__