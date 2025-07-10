#include "dbg_util_def.h"

#ifdef DBGUTIL_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cassert>
#include <cinttypes>

#include "dbgutil_log_imp.h"
#include "os_util.h"
#include "win32_pe_reader.h"

namespace dbgutil {

static Logger sLogger;

Win32PEReader::Win32PEReader() : m_isPE32Plus(false) {}

void Win32PEReader::resetData() {
    m_isPE32Plus = false;
    m_relocBase = 0;
    m_sections.clear();
}

DbgUtilErr Win32PEReader::readImage() {
    // image begins with DOS stub
    // at offset 0x3c there is the offset to the PE signature
    // it is not documented how many bytes comprise the offset
    // it will be found by trial and error
    DbgUtilErr rc = m_fileReader.seek(0x3C);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    uint32_t peOffset = 0;
    rc = m_fileReader.read(peOffset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    LOG_DEBUG(sLogger, "PE data for module %s starts at offset %u", m_imagePath.c_str(), peOffset);

    // seek to PE offset
    rc = m_fileReader.seek(peOffset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // read PW signature word
    const uint32_t PE_SIG_SIZE = 4;
    char sig[PE_SIG_SIZE] = {};
    rc = m_fileReader.readFull(sig, PE_SIG_SIZE);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // now verify we have the PE signature at the specified offset
    if (sig[0] != 'P' || sig[1] != 'E' || sig[2] != 0 || sig[3] != 0) {
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    // now we can take a look at the COFF header
    rc = m_fileReader.read(m_fileHeader);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // check machine is supported
    WORD machine = m_fileHeader.Machine;
    if (machine != IMAGE_FILE_MACHINE_AMD64 && machine != IMAGE_FILE_MACHINE_I386) {
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    // must be an executable image
    if ((m_fileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) == 0) {
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    // skip system files
    if ((m_fileHeader.Characteristics & IMAGE_FILE_SYSTEM) != 0) {
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    // skip if no symbols are found
    if (m_fileHeader.PointerToSymbolTable == 0) {
        return DBGUTIL_ERR_INVALID_ARGUMENT;
    }

    // skip if no optional header found (this is an error indicating corrupt file)
    if (m_fileHeader.SizeOfOptionalHeader == 0) {
        return DBGUTIL_ERR_DATA_CORRUPT;
    }

    // check for 32/64 image type
    m_is64Bit = ((m_fileHeader.Characteristics & IMAGE_FILE_32BIT_MACHINE) == 0);

    // check for exe/dll
    m_isExe = ((m_fileHeader.Characteristics & IMAGE_FILE_DLL) == 0);

    // remember offset (required for optional header scanning)
    uint64_t headersOffset = 0;
    rc = m_fileReader.getOffset(headersOffset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // check for PE format type (must be PE32 or PE32+)
    WORD magic = 0;
    rc = m_fileReader.read(magic);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        m_isPE32Plus = true;
    } else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        m_isPE32Plus = false;
    } else {
        return DBGUTIL_ERR_NOT_IMPLEMENTED;
    }

    // we need to get string table as soon as possible, for inferring names
    uint32_t strTabOffset =
        m_fileHeader.PointerToSymbolTable + sizeof(IMAGE_SYMBOL) * m_fileHeader.NumberOfSymbols;

    // seek to string table
    rc = m_fileReader.seek(strTabOffset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // read string table size (specified in bytes)
    // NOTE: size includes size field itself
    char strTabSizeBuf[4];
    rc = m_fileReader.readFull(strTabSizeBuf, 4);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    uint32_t strTabSize = *(uint32_t*)strTabSizeBuf;
    LOG_DEBUG(sLogger, "String table size: %" PRIu64, strTabSize);

    // now read entire string table (will be discarded later)
    // NOTE: string offsets used in PE image assume string table starts with 4-bytes table size, so
    // we first seek back 4 bytes
    rc = m_fileReader.seek(strTabOffset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // NOTE: reading also first 4-bytes of string table size
    m_strTab.resize(strTabSize);
    rc = m_fileReader.readFull(&m_strTab[0], strTabSize);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // now seek back to headers offset
    rc = m_fileReader.seek(headersOffset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    // scan headers
    rc = m_isPE32Plus ? scanHdrs64() : scanHdrs32();
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    buildSymTab();

    // discard string table, it is not required anymore
    m_strTab.clear();
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32PEReader::scanHdrs32() {
    // optional header is found right after file header
    IMAGE_OPTIONAL_HEADER32 optHdr = {};
    DbgUtilErr rc = m_fileReader.read(optHdr);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    m_relocBase = optHdr.ImageBase;

    // section headers follow optional header
    return readSectionHeaders();
}

DbgUtilErr Win32PEReader::scanHdrs64() {
    // optional header is found right after file header
    IMAGE_OPTIONAL_HEADER64 optHdr = {};
    DbgUtilErr rc = m_fileReader.read(optHdr);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    m_relocBase = optHdr.ImageBase;

    // section headers follow optional header
    return readSectionHeaders();
}

DbgUtilErr Win32PEReader::readSectionHeaders() {
    std::vector<IMAGE_SECTION_HEADER> sectionHeaders(m_fileHeader.NumberOfSections);
    DbgUtilErr rc = m_fileReader.readFull(
        (char*)&sectionHeaders[0], sizeof(IMAGE_SECTION_HEADER) * m_fileHeader.NumberOfSections);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }
    for (uint32_t i = 0; i < m_fileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER* secHdr = &sectionHeaders[i];
        std::string secName;
        DbgUtilErr rc = getSectionName(secHdr->Name, secName);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        LOG_DEBUG(sLogger, "Section %s starts at file offset %u (virtual offset %u), with size: %u",
                  secName.c_str(), secHdr->PointerToRawData, secHdr->VirtualAddress,
                  secHdr->Misc.VirtualSize);
        if (!m_sectionMap
                 .insert(OsSectionMap::value_type(
                     secName, {secName, secHdr->PointerToRawData, secHdr->Misc.VirtualSize, 0}))
                 .second) {
            return DBGUTIL_ERR_DATA_CORRUPT;
        }
        m_sections.push_back(
            {secName, secHdr->PointerToRawData, secHdr->VirtualAddress, secHdr->Misc.VirtualSize});
    }
    m_miniSections.resize(m_sections.size());
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32PEReader::buildSymTab() {
    DWORD symTabOffset = m_fileHeader.PointerToSymbolTable;
    uint32_t symCount = m_fileHeader.NumberOfSymbols;

    // symbol name: short name is used if there is a null at the end, otherwise low 4 bytes are
    // zero, and high 4 bytes are offset to string table

    // section value: 1-based index into the section table. special values: 0 - no section assigned.
    // If value is zero then this is an external value. A non-zero value specifies common symbol
    // size. section value (-1) means absolute value (not an offset/address). section value (-2)
    // means type or debug info. Can be used .file records.

    // symbol types: LSB is base type, MSB is complex type
    // only IMAGE_SYM_DTYPE_FUNCTION is of interest here

    // storage class: external - if section number is 0, the value indicates size, otherwise value
    // indicates offset into section. static - value indicates offset into section. If value is zero
    // then the symbol is a section name. function - for .ef records the value indicates the size of
    // the function code. file - source file name, followed by auxiliary records.

    DbgUtilErr rc = m_fileReader.seek(symTabOffset);
    if (rc != DBGUTIL_ERR_OK) {
        return rc;
    }

    uint32_t srcFileIndex = 0;
    IMAGE_SYMBOL sym = {};
    for (uint32_t i = 0; i < symCount; ++i) {
        rc = m_fileReader.read(sym);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        // check for file name entry
        uint32_t auxRecCount = sym.NumberOfAuxSymbols;
        uint32_t auxRecRead = 0;
        if (sym.StorageClass == IMAGE_SYM_CLASS_FILE) {
            // we expect one auxiliary record with file name
            if (auxRecCount == 1) {
                std::string fileName;
                IMAGE_AUX_SYMBOL auxSym = {};
                rc = m_fileReader.read(auxSym);
                if (rc != DBGUTIL_ERR_OK) {
                    return rc;
                }
                ++auxRecRead;
                rc = getFileName(auxSym.File.Name, fileName);
                if (rc != DBGUTIL_ERR_OK) {
                    LOG_ERROR(sLogger,
                              "Failed to build symbol table, unable to retrieve file name");
                    return rc;
                }
                LOG_DEBUG(sLogger, "Found file: %s", fileName.c_str());
                srcFileIndex = (uint32_t)m_srcFileNames.size();
                m_srcFileNames.push_back(fileName);
            }
        }

        // check for mini-section ref
        else if (sym.StorageClass == IMAGE_SYM_CLASS_STATIC && sym.Type == IMAGE_SYM_TYPE_NULL) {
            // the section must be positive and the value must be positive
            if (sym.SectionNumber > 0 && sym.Value > 0) {
                // now get the auxiliary record which has the function code size
                if (auxRecCount == 1) {
                    // this is totally speculative reverse engineering hack
                    // could not find documentation for this...
                    IMAGE_AUX_SYMBOL auxSym = {};
                    rc = m_fileReader.read(auxSym);
                    if (rc != DBGUTIL_ERR_OK) {
                        return rc;
                    }
                    ++auxRecRead;
                    m_miniSections[sym.SectionNumber - 1].insert(
                        FuncSizeMap::value_type(sym.Value, auxSym.Section.Length));
                }
                // NOTE: when symbol offset is not present, we simply stretch to the next symbol
                // boundary, or to the end of the section
            }
        }

        // check for function
        else if ((sym.Type >> 4) == IMAGE_SYM_DTYPE_FUNCTION) {
            // we distinguish mini-section from function by
            std::string name;
            getSymbolName(sym.N.ShortName, sym.N.LongName, name);
            if (/*sym.Value != 0 &&*/ sym.SectionNumber > 0) {
                // offset to some section, get offset relative to file start
                if (sym.SectionNumber >= m_sections.size()) {
                    LOG_DEBUG(sLogger, "WARN: Symbol %s referring invalid section %u", name.c_str(),
                              (unsigned)sym.SectionNumber);
                    continue;
                }
                uint64_t symOffset = m_sections[sym.SectionNumber - 1].m_virtualOffset + sym.Value;
                uint64_t symSize = 0;

                // check if we have function length in mini-section map
                FuncSizeMap& funcSizeMap = m_miniSections[sym.SectionNumber - 1];
                FuncSizeMap::iterator itr = funcSizeMap.find(sym.Value);
                if (itr != funcSizeMap.end()) {
                    symSize = itr->second;
                }

                // insert symbol
                m_symInfoSet.push_back(
                    {symOffset, symSize, name, srcFileIndex, sym.SectionNumber - 1});
                LOG_DIAG(sLogger, "Found function: %s, %u-%u, %s", name.c_str(),
                         (unsigned)symOffset, (unsigned)(symOffset + symSize),
                         m_srcFileNames[srcFileIndex].c_str());
            }
        }

        // move to next symbol, skip its auxiliary records
        rc = m_fileReader.skip((auxRecCount - auxRecRead) * sizeof(IMAGE_AUX_SYMBOL));
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        symCount -= (auxRecCount - auxRecRead);
    }

    // sort symbol table
    std::sort(m_symInfoSet.begin(), m_symInfoSet.end());

    // fix all symbols with size zero, so search is easier
    // NOTE: we might go beyond actual symbol size, but there is no other way to tell
    for (uint32_t i = 0; i < m_symInfoSet.size(); ++i) {
        OsSymbolInfo& symInfo = m_symInfoSet[i];
        if (symInfo.m_size == 0) {
            if (i + 1 < m_symInfoSet.size()) {
                symInfo.m_size = m_symInfoSet[i + 1].m_offset - symInfo.m_offset;
            } else {
                // last symbol boundary stretched to end of origin section
                SectionInfo& originSection = m_sections[symInfo.m_originSectionIndex];
                uint64_t sectionEndOffset = originSection.m_virtualOffset + originSection.m_size;
                symInfo.m_size = sectionEndOffset - symInfo.m_offset;
            }
        }
        LOG_DEBUG(sLogger, "Function at %p - %p %s", (void*)symInfo.m_offset,
                  (void*)(symInfo.m_offset + symInfo.m_size), symInfo.m_name.c_str());
    }

    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32PEReader::getSectionName(unsigned char* nameRef, std::string& name) {
    if (nameRef[0] == '/') {
        // reference to string table
        int strOff = std::stoi((char*)nameRef + 1);
        if (strOff >= m_strTab.size()) {
            return DBGUTIL_ERR_DATA_CORRUPT;
        }
        name = &m_strTab[strOff];
        return DBGUTIL_ERR_OK;
    }

    if (nameRef[7] == 0) {
        // null padded UTF-8 string (just copy, assuming all chars are ascii...)
        name = (char*)nameRef;
    } else {
        // no null, so we carefully first copy into a null terminate string
        const size_t BUF_SIZE = 9;
        char tmp[BUF_SIZE];
        errno_t errc = strncpy_s(tmp, BUF_SIZE, (const char*)nameRef, 8);
        if (errc == 0) {
            tmp[8] = 0;
            name = tmp;
        } else {
            LOG_ERROR(sLogger, "Failed to securely copy section name");
            return DBGUTIL_ERR_INTERNAL_ERROR;
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32PEReader::getSymbolName(unsigned char* shortName, unsigned long* longName,
                                        std::string& name) {
    if (longName[0] == 0) {
        if (longName[1] >= m_strTab.size()) {
            return DBGUTIL_ERR_INVALID_ARGUMENT;
        }
        name = &m_strTab[longName[1]];
    } else {
        if (shortName[7] == 0) {
            name = (char*)shortName;
        } else {
            // carefully copy
            const size_t BUF_SIZE = 9;
            char tmp[BUF_SIZE];
            errno_t errc = strncpy_s(tmp, BUF_SIZE, (const char*)shortName, 8);
            if (errc == 0) {
                tmp[8] = 0;
                name = tmp;
            } else {
                LOG_ERROR(sLogger, "Failed to securely copy symbol name");
                return DBGUTIL_ERR_INTERNAL_ERROR;
            }
        }
    }
    return DBGUTIL_ERR_OK;
}

DbgUtilErr Win32PEReader::getFileName(unsigned char* rawName, std::string& name) {
    if (rawName[IMAGE_SIZEOF_SYMBOL - 1] == 0) {
        name = (char*)rawName;
    } else {
        // carefully copy
        char tmp[IMAGE_SIZEOF_SYMBOL + 1];
        errno_t errc =
            strncpy_s(tmp, IMAGE_SIZEOF_SYMBOL + 1, (const char*)rawName, IMAGE_SIZEOF_SYMBOL);
        if (errc == 0) {
            tmp[IMAGE_SIZEOF_SYMBOL] = 0;
            name = tmp;
        } else {
            LOG_ERROR(sLogger, "Failed to securely copy file name");
            return DBGUTIL_ERR_INTERNAL_ERROR;
        }
    }
    return DBGUTIL_ERR_OK;
}

class Win32PEReaderFactory : public OsImageReaderFactory {
public:
    /** @brief Creates the singleton instance of the factory. */
    static void createInstance() {
        assert(sInstance == nullptr);
        sInstance = new (std::nothrow) Win32PEReaderFactory();
        assert(sInstance != nullptr);
    }

    /** @brief Retrieves a reference to the single instance of the factory. */
    static Win32PEReaderFactory* getInstance() {
        assert(sInstance != nullptr);
        return sInstance;
    }

    /** @brief Destroys the singleton instance of the factory. */
    static void destroyInstance() {
        assert(sInstance != nullptr);
        delete sInstance;
        sInstance = nullptr;
    }

    Win32PEReaderFactory(const Win32PEReaderFactory&) = delete;
    Win32PEReaderFactory(Win32PEReaderFactory&&) = delete;

    OsImageReader* createImageReader() final { return new (std::nothrow) Win32PEReader(); }

private:
    Win32PEReaderFactory() {}
    ~Win32PEReaderFactory() final {}

    static Win32PEReaderFactory* sInstance;
};

Win32PEReaderFactory* Win32PEReaderFactory::sInstance = nullptr;

extern DbgUtilErr initWin32PEReader() {
    registerLogger(sLogger, "win32_pe_reader");
    Win32PEReaderFactory::createInstance();
    setImageReaderFactory(Win32PEReaderFactory::getInstance());
    return DBGUTIL_ERR_OK;
}

extern DbgUtilErr termWin32PEReader() {
    setImageReaderFactory(nullptr);
    Win32PEReaderFactory::destroyInstance();
    unregisterLogger(sLogger);
    return DBGUTIL_ERR_OK;
}

#if 0
BEGIN_STARTUP_JOB(OsImageReaderFactory) {
    Win32PEReaderFactory::createInstance();
    setImageReaderFactory(Win32PEReaderFactory::getInstance());
    return DBGUTIL_ERR_OK;
}
END_STARTUP_JOB(OsImageReaderFactory)

BEGIN_TEARDOWN_JOB(OsImageReaderFactory) {
    setImageReaderFactory(nullptr);
    Win32PEReaderFactory::destroyInstance();
    return DBGUTIL_ERR_OK;
}
END_TEARDOWN_JOB(OsImageReaderFactory)
#endif

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS