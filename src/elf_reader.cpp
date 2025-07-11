#include "elf_reader.h"

#ifdef DBGUTIL_LINUX

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include "dbgutil_log_imp.h"
#include "os_util.h"

namespace dbgutil {

static Logger sLogger;

ElfReader::ElfReader() : m_symTabSize(0), m_symEntrySize(0) {}

void ElfReader::resetData() {
    m_symTabSize = 0;
    m_symEntrySize = 0;
}

DbgUtilErr ElfReader::readImage() {
    // verify header
    DbgUtilErr rc = verifyHeader();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    LOG_DEBUG(sLogger, "ELF header verified");

    if (canLog(sLogger, LS_DEBUG)) {
        dumpSectionHeaders();
    }

    // get special section pointers
    rc = getSectionHeaderStrTab();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // build section map
    rc = buildSectionMap();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    rc = getStrTab();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    rc = getSymTab();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    rc = buildSymInfoSet();
    m_symTab.clear();
    return rc;
}

DbgUtilErr ElfReader::verifyHeader() {
    unsigned char e_ident[EI_NIDENT] = {};
    DbgUtilErr rc = m_fileReader.readFull((char*)e_ident, EI_NIDENT);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    if (strncmp((const char*)e_ident, (const char*)ELFMAG, SELFMAG) != 0) {
        // invalid magic number
        LOG_ERROR(sLogger, "Binary ELF image magic mismatch");
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    // check class
    if (e_ident[EI_CLASS] != ELFCLASS32 && e_ident[EI_CLASS] != ELFCLASS64) {
        LOG_ERROR(sLogger, "Unsupported ELF class");
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }
    m_is64Bit = (e_ident[EI_CLASS] == ELFCLASS64);

    // check ABI (what is it for MinGW??)
    if (e_ident[EI_OSABI] != ELFOSABI_NONE && e_ident[EI_OSABI] != ELFOSABI_LINUX) {
        LOG_ERROR(sLogger, "Unsupported ELF ABI");
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    // check for symbol table
    return checkHeader();
}

DbgUtilErr ElfReader::checkHeader() {
    DbgUtilErr rc = m_fileReader.seek(0);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    if (m_is64Bit) {
        rc = m_fileReader.read(m_hdr.m_hdr64);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        return checkHeader64(&m_hdr.m_hdr64);
    } else {
        rc = m_fileReader.read(m_hdr.m_hdr32);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        return checkHeader32(&m_hdr.m_hdr32);
    }
}

DbgUtilErr ElfReader::checkHeader32(Elf32_Ehdr* hdr) {
    // check executable image
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) {
        LOG_ERROR(sLogger, "Unsupported image type (nor executable, neither shared object)");
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }
    m_isExe = hdr->e_type == ET_EXEC;

    // check arch is x86
    if (hdr->e_machine != EM_386 && hdr->e_machine != EM_X86_64) {
        LOG_ERROR(sLogger, "Unsupported target machine");
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    // check version
    if (hdr->e_version != EV_CURRENT) {
        LOG_ERROR(sLogger, "ELF header version mismatch");
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr ElfReader::checkHeader64(Elf64_Ehdr* hdr) {
    // check executable image
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) {
        LOG_ERROR(sLogger, "Unsupported image type (nor executable, neither shared object)");
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }
    m_isExe = hdr->e_type == ET_EXEC;

    // check arch is x86
    if (hdr->e_machine != EM_386 && hdr->e_machine != EM_X86_64) {
        LOG_ERROR(sLogger, "Unsupported target machine");
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    // check version
    if (hdr->e_version != EV_CURRENT) {
        LOG_ERROR(sLogger, "ELF header version mismatch");
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr ElfReader::buildSectionMap() {
    return m_is64Bit ? buildSectionMap64(&m_hdr.m_hdr64) : buildSectionMap32(&m_hdr.m_hdr32);
}

DbgUtilErr ElfReader::buildSectionMap32(Elf32_Ehdr* hdr) {
    // run through sections (skip first section, which is void)
    DbgUtilErr rc = m_fileReader.seek(hdr->e_shoff + hdr->e_shentsize);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    for (int i = 1; i < (int)hdr->e_shnum; ++i) {
        Elf32_Shdr secHdr = {};
        rc = m_fileReader.read(secHdr);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        char* secName = &m_shStrTab[secHdr.sh_name];
        uint64_t secOffset = secHdr.sh_offset;
        LOG_DEBUG(sLogger, "Adding section %p - %p %s", (void*)secOffset,
                  (void*)(secOffset + secHdr.sh_size), secName);
        bool res = m_sectionMap
                       .insert(OsSectionMap::value_type(
                           secName, {secName, secOffset, secHdr.sh_size, nullptr}))
                       .second;
        if (!res) {
            return DBGUTIL_ERR_DATA_CORRUPT;
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr ElfReader::buildSectionMap64(Elf64_Ehdr* hdr) {
    // run through sections (skip first section, which is void)
    DbgUtilErr rc = m_fileReader.seek(hdr->e_shoff + hdr->e_shentsize);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    for (int i = 1; i < (int)hdr->e_shnum; ++i) {
        Elf64_Shdr secHdr = {};
        rc = m_fileReader.read(secHdr);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        char* secName = &m_shStrTab[secHdr.sh_name];
        uint64_t secOffset = secHdr.sh_offset;
        LOG_DEBUG(sLogger, "Adding section %p - %p %s", (void*)secOffset,
                  (void*)(secOffset + secHdr.sh_size), secName);
        bool res = m_sectionMap
                       .insert(OsSectionMap::value_type(
                           secName, {secName, secOffset, secHdr.sh_size, nullptr}))
                       .second;
        if (!res) {
            return DBGUTIL_ERR_DATA_CORRUPT;
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr ElfReader::getSectionHeaderStrTab() {
    return m_is64Bit ? getSectionHeaderStrTab64(&m_hdr.m_hdr64)
                     : getSectionHeaderStrTab32(&m_hdr.m_hdr32);
}

DbgUtilErr ElfReader::getSectionHeaderStrTab32(Elf32_Ehdr* hdr) {
    // skip to start of section header of string table for section headers
    // NOTE: section index is 1-based
    uint32_t offset = hdr->e_shoff + hdr->e_shstrndx * hdr->e_shentsize;
    DbgUtilErr rc = m_fileReader.seek(offset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    Elf32_Shdr secHdr = {};
    rc = m_fileReader.read(secHdr);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    if (secHdr.sh_type != SHT_STRTAB) {
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    // seek to offset and read entire section
    rc = m_fileReader.seek(secHdr.sh_offset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    m_shStrTab.resize(secHdr.sh_size);
    return m_fileReader.readFull(&m_shStrTab[0], secHdr.sh_size);
}

DbgUtilErr ElfReader::getSectionHeaderStrTab64(Elf64_Ehdr* hdr) {
    // skip to start of section header of string table for section headers
    // NOTE: section index is 1-based
    uint32_t offset = hdr->e_shoff + hdr->e_shstrndx * hdr->e_shentsize;
    DbgUtilErr rc = m_fileReader.seek(offset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    Elf64_Shdr secHdr = {};
    rc = m_fileReader.read(secHdr);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    if (secHdr.sh_type != SHT_STRTAB) {
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    // seek to offset and read entire section
    rc = m_fileReader.seek(secHdr.sh_offset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    m_shStrTab.resize(secHdr.sh_size);
    return m_fileReader.readFull(&m_shStrTab[0], secHdr.sh_size);
}

DbgUtilErr ElfReader::getStrTab() {
    return m_is64Bit ? getStrTab64(&m_hdr.m_hdr64) : getStrTab32(&m_hdr.m_hdr32);
}

DbgUtilErr ElfReader::getStrTab32(Elf32_Ehdr* hdr) {
    // run through sections (skip first section, which is void)
    DbgUtilErr rc = m_fileReader.seek(hdr->e_shoff + hdr->e_shentsize);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    for (int i = 1; i < (int)hdr->e_shnum; ++i) {
        Elf32_Shdr secHdr = {};
        rc = m_fileReader.read(secHdr);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }

        // check especially for correct section name
        if (secHdr.sh_type == SHT_STRTAB && strcmp(&m_shStrTab[secHdr.sh_name], ".strtab") == 0) {
            rc = m_fileReader.seek(secHdr.sh_offset);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
            m_strTab.resize(secHdr.sh_size);
            return m_fileReader.readFull(&m_strTab[0], secHdr.sh_size);
        }
    }
    return DBGUTIL_ERR_NOT_FOUND;
}

DbgUtilErr ElfReader::getStrTab64(Elf64_Ehdr* hdr) {
    // run through sections (skip first section, which is void)
    DbgUtilErr rc = m_fileReader.seek(hdr->e_shoff + hdr->e_shentsize);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    for (int i = 1; i < (int)hdr->e_shnum; ++i) {
        Elf64_Shdr secHdr = {};
        rc = m_fileReader.read(secHdr);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }

        // check especially for correct section name
        if (secHdr.sh_type == SHT_STRTAB && strcmp(&m_shStrTab[secHdr.sh_name], ".strtab") == 0) {
            rc = m_fileReader.seek(secHdr.sh_offset);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
            m_strTab.resize(secHdr.sh_size);
            return m_fileReader.readFull(&m_strTab[0], secHdr.sh_size);
        }
    }
    return DBGUTIL_ERR_NOT_FOUND;
}

DbgUtilErr ElfReader::getSymTab() {
    return m_is64Bit ? getSymTab64(&m_hdr.m_hdr64) : getSymTab32(&m_hdr.m_hdr32);
}

DbgUtilErr ElfReader::getSymTab32(Elf32_Ehdr* hdr) {
    // run through sections (skip first section, which is void)
    DbgUtilErr rc = m_fileReader.seek(hdr->e_shoff + hdr->e_shentsize);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    for (int i = 1; i < (int)hdr->e_shnum; ++i) {
        Elf32_Shdr secHdr = {};
        rc = m_fileReader.read(secHdr);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }

        if (secHdr.sh_type == SHT_SYMTAB) {
            m_symTabSize = secHdr.sh_size;
            m_symEntrySize = secHdr.sh_entsize;

            // seek to symbol table offset and read entire table into buffer
            rc = m_fileReader.seek(secHdr.sh_offset);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
            m_symTab.resize(secHdr.sh_size);
            return m_fileReader.readFull(&m_symTab[0], secHdr.sh_size);
        }
    }
    return DBGUTIL_ERR_NOT_FOUND;
}

DbgUtilErr ElfReader::getSymTab64(Elf64_Ehdr* hdr) {
    // run through sections (skip first section, which is void)
    DbgUtilErr rc = m_fileReader.seek(hdr->e_shoff + hdr->e_shentsize);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    for (int i = 0; i < (int)hdr->e_shnum; ++i) {
        Elf64_Shdr secHdr = {};
        rc = m_fileReader.read(secHdr);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }

        if (secHdr.sh_type == SHT_SYMTAB) {
            m_symTabSize = secHdr.sh_size;
            m_symEntrySize = secHdr.sh_entsize;

            // seek to symbol table offset and read entire table into buffer
            rc = m_fileReader.seek(secHdr.sh_offset);
            if (rc != DBGUTIL_ERR_OK) {
                return rc;
            }
            m_symTab.resize(secHdr.sh_size);
            return m_fileReader.readFull(&m_symTab[0], secHdr.sh_size);
        }
    }
    return DBGUTIL_ERR_NOT_FOUND;
}

DbgUtilErr ElfReader::buildSymInfoSet() {
    return m_is64Bit ? buildSymInfoSet64(&m_hdr.m_hdr64) : buildSymInfoSet32(&m_hdr.m_hdr32);
}

DbgUtilErr ElfReader::buildSymInfoSet32(Elf32_Ehdr* hdr) {
    char* symTabStart = &m_symTab[0];
    char* symSecEndPos = symTabStart + m_symTabSize;
    char* symPos = symTabStart;
    uint32_t srcFileIndex = 0;
    for (; symPos < symSecEndPos; symPos += m_symEntrySize) {
        Elf32_Sym* symInfo = (Elf32_Sym*)symPos;
        if (symInfo->st_shndx == SHN_UNDEF) {
            // external symbol, not relevant
            continue;
        }

        // we save only functions, but we remember the source file if possible
        const char* symName = &m_strTab[symInfo->st_name];
        int type = ELF32_ST_TYPE(symInfo->st_info);
        if (type == STT_FILE) {
            srcFileIndex = (uint32_t)m_srcFileNames.size();
            m_srcFileNames.push_back(symName);
            continue;
        }
        if (type != STT_FUNC) {
            continue;
        }

        // internal symbol
        uint64_t symOff = symInfo->st_value;
        uint64_t symSize = (uint64_t)symInfo->st_size;
        m_symInfoSet.push_back({symOff, symSize, symName, srcFileIndex});
        LOG_DEBUG(sLogger, "Found function %p - %p %s", (void*)symOff, (void*)(symOff + symSize),
                  symName);
    }
    std::sort(m_symInfoSet.begin(), m_symInfoSet.end());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr ElfReader::buildSymInfoSet64(Elf64_Ehdr* hdr) {
    char* symTabStart = &m_symTab[0];
    char* symSecEndPos = symTabStart + m_symTabSize;
    char* symPos = symTabStart;
    uint32_t srcFileIndex = 0;
    for (; symPos < symSecEndPos; symPos += m_symEntrySize) {
        Elf64_Sym* symInfo = (Elf64_Sym*)symPos;
        if (symInfo->st_shndx == SHN_UNDEF) {
            // external symbol, not relevant
            continue;
        }

        // we save only functions, but we remember the source file if possible
        const char* symName = &m_strTab[symInfo->st_name];
        int type = ELF64_ST_TYPE(symInfo->st_info);
        if (type == STT_FILE) {
            srcFileIndex = (uint32_t)m_srcFileNames.size();
            m_srcFileNames.push_back(symName);
            LOG_DEBUG(sLogger, "Found file: %s (name index: %u, file index: %u)", symName,
                      symInfo->st_name, srcFileIndex);
            continue;
        }
        if (type != STT_FUNC) {
            continue;
        }

        // internal symbol
        uint64_t symOff = symInfo->st_value;
        uint64_t symSize = (uint64_t)symInfo->st_size;
        m_symInfoSet.push_back({symOff, symSize, symName, srcFileIndex});
        LOG_DEBUG(sLogger, "Found function %p - %p %s", (void*)symOff, (void*)(symOff + symSize),
                  symName);
    }
    std::sort(m_symInfoSet.begin(), m_symInfoSet.end());
    return DBGUTIL_ERR_OK;
}

void ElfReader::dumpSectionHeaders() {
    if (m_is64Bit) {
        dumpSectionHeaders64();
    } else {
        dumpSectionHeaders32();
    }
}

void ElfReader::dumpSectionHeaders32() {
    Elf32_Ehdr* hdr = &m_hdr.m_hdr32;
    DbgUtilErr rc = m_fileReader.seek(hdr->e_shoff + hdr->e_shentsize);
    if (rc != DBGUTIL_ERR_OK) {
        return;
    }

    for (int i = 1; i < (int)hdr->e_shnum; ++i) {
        Elf32_Shdr secHdr = {};
        rc = m_fileReader.read(secHdr);
        if (rc != DBGUTIL_ERR_OK) {
            return;
        }
        LOG_DEBUG(sLogger, "%d name=%u, offset=%u, size=%u", i, secHdr.sh_name, secHdr.sh_offset,
                  secHdr.sh_size);
    }
}

void ElfReader::dumpSectionHeaders64() {
    Elf64_Ehdr* hdr = &m_hdr.m_hdr64;
    DbgUtilErr rc = m_fileReader.seek(hdr->e_shoff + hdr->e_shentsize);
    if (rc != DBGUTIL_ERR_OK) {
        return;
    }

    for (int i = 1; i < (int)hdr->e_shnum; ++i) {
        Elf64_Shdr secHdr = {};
        rc = m_fileReader.read(secHdr);
        if (rc != DBGUTIL_ERR_OK) {
            return;
        }
        LOG_DEBUG(sLogger, "%d name=%u, offset=%u, size=%u", i, secHdr.sh_name, secHdr.sh_offset,
                  secHdr.sh_size);
    }
}

class ElfReaderFactory : public OsImageReaderFactory {
public:
    /** @brief Creates the singleton instance of the factory. */
    static void createInstance() {
        assert(sInstance == nullptr);
        sInstance = new (std::nothrow) ElfReaderFactory();
        assert(sInstance != nullptr);
    }

    /** @brief Retrieves a reference to the single instance of the factory. */
    static ElfReaderFactory* getInstance() {
        assert(sInstance != nullptr);
        return sInstance;
    }

    /** @brief Destroys the singleton instance of the factory. */
    static void destroyInstance() {
        assert(sInstance != nullptr);
        delete sInstance;
        sInstance = nullptr;
    }

    ElfReaderFactory(const ElfReaderFactory&) = delete;
    ElfReaderFactory(ElfReaderFactory&&) = delete;

    OsImageReader* createImageReader() final { return new (std::nothrow) ElfReader(); }

private:
    ElfReaderFactory() {}
    ~ElfReaderFactory() final {}

    static ElfReaderFactory* sInstance;
};

ElfReaderFactory* ElfReaderFactory::sInstance = nullptr;

DbgUtilErr initElfReader() {
    registerLogger(sLogger, "elf_reader");
    ElfReaderFactory::createInstance();
    setImageReaderFactory(ElfReaderFactory::getInstance());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr termElfReader() {
    setImageReaderFactory(nullptr);
    ElfReaderFactory::destroyInstance();
    return DBGUTIL_ERR_OK;
}

}  // namespace dbgutil

#endif  // DBGUTIL_LINUX