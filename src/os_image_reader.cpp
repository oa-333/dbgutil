#include "os_image_reader.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstring>

#include "dbgutil_log_imp.h"
#include "os_util.h"

namespace libdbg {

static Logger sLogger;

static OsImageReaderFactory* sFactory = nullptr;

void OsImageReader::initLogger() { registerLogger(sLogger, "os_image_reader"); }
void OsImageReader::termLogger() { unregisterLogger(sLogger); }

OsImageReader::OsImageReader()
    : m_fileSizeBytes(0), m_moduleBase(nullptr), m_is64Bit(false), m_isExe(false), m_relocBase(0) {}

LibDbgErr OsImageReader::open(const char* path, void* moduleBase) {
    m_imagePath = path;
    LibDbgErr rc = m_fileReader.open(path);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }
    m_moduleBase = (char*)moduleBase;
    LOG_DEBUG(sLogger, "Processing binary image: %s", path);

    rc = readImage();
    if (rc != LIBDBG_ERR_OK) {
        m_fileReader.close();
    }
    return rc;
}

void OsImageReader::close() {
    if (m_fileReader.isOpen()) {
        m_fileReader.close();
    }
    m_imagePath.clear();
    m_fileSizeBytes = 0;
    m_moduleBase = nullptr;
    m_is64Bit = false;
    m_isExe = false;
    m_symInfoSet.clear();
    m_srcFileNames.clear();
    m_sectionMap.clear();
    resetData();
}

LibDbgErr OsImageReader::searchSymbol(void* symAddress, std::string& symName, std::string& fileName,
                                      void** address) {
    // scan symbol table for relative address
    if (symAddress < m_moduleBase) {
        LOG_DEBUG(sLogger, "Attempt to search symbol %p in module starting at %p: out of range",
                  symAddress, m_moduleBase);
        return LIBDBG_ERR_INVALID_ARGUMENT;
    }
    uint64_t symOff = (uint64_t)(((char*)symAddress) - ((char*)m_moduleBase));
    LOG_DEBUG(sLogger, "Searching for symbol %p at offset %u", symAddress, (unsigned)symOff);
    SymInfoSet::iterator itr =
        std::lower_bound(m_symInfoSet.begin(), m_symInfoSet.end(), symOff,
                         [](const OsSymbolInfo& symInfo, uint64_t symOff) {
                             // stop search when end point of interval surpasses searched offset
                             return symInfo.m_offset + symInfo.m_size <= symOff;
                         });

    if (itr == m_symInfoSet.end()) {
        LOG_DEBUG(sLogger, "Symbol not found");
        return LIBDBG_ERR_NOT_FOUND;
    }

    const OsSymbolInfo& symInfo = *itr;
    if (symInfo.contains(symOff)) {
        symName = symInfo.m_name;
        fileName = m_srcFileNames[symInfo.m_srcFileIndex];
        *address = (void*)(m_moduleBase + symInfo.m_offset);
        LOG_DEBUG(sLogger, "Found symbol %s at start address %p, file %s", symName.c_str(),
                  *address, fileName.c_str());
        return LIBDBG_ERR_OK;
    }
    return LIBDBG_ERR_NOT_FOUND;
}

LibDbgErr OsImageReader::getSection(const char* name, OsImageSection& section) {
    OsSectionMap::const_iterator itr = m_sectionMap.find(name);
    if (itr == m_sectionMap.end()) {
        return LIBDBG_ERR_NOT_FOUND;
    }
    section = itr->second;
    if (section.m_start == nullptr) {
        // materialize section on-demand
        return materializeSection(section);
    }
    return LIBDBG_ERR_OK;
}

LibDbgErr OsImageReader::getSections(const char* prefix, std::vector<OsImageSection>& sections) {
    std::string strPrefix = (prefix == nullptr) ? "" : prefix;
    OsSectionMap::iterator itr = m_sectionMap.begin();
    while (itr != m_sectionMap.end()) {
#if defined(__cpp_lib_starts_ends_with) && __cpp_lib_starts_ends_with >= 201711L
        if (itr->first.starts_with(strPrefix)) {
#else
        if (itr->first.rfind(strPrefix, 0) == 0) {
#endif
            OsImageSection& section = itr->second;
            if (section.m_start == nullptr) {
                // materialize section on-demand
                LibDbgErr rc = materializeSection(section);
                if (rc != LIBDBG_ERR_OK) {
                    return rc;
                }
            }
            sections.push_back(section);
        }
        ++itr;
    }

    return LIBDBG_ERR_OK;
}

LibDbgErr OsImageReader::materializeSection(OsImageSection& section) {
    // first insert a mapping, so we can read data in-place into the buffer
    std::pair<MaterializedSectionMap::iterator, bool> itrRes = m_materializedSectionMap.insert(
        MaterializedSectionMap::value_type(section.m_name, SectionBuffer()));
    if (!itrRes.second) {
        return LIBDBG_ERR_INTERNAL_ERROR;
    }

    // seek to section start
    LibDbgErr rc = m_fileReader.seek(section.m_offset);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }

    // get reference to section buffer, and reserve bytes
    SectionBuffer& sectionBuffer = itrRes.first->second;
    sectionBuffer.resize(section.m_size);

    // read entire section
    rc = m_fileReader.readFull(&sectionBuffer[0], section.m_size);
    if (rc != LIBDBG_ERR_OK) {
        return rc;
    }

    // update section object and return
    section.m_start = &sectionBuffer[0];
    return LIBDBG_ERR_OK;
}

void setImageReaderFactory(OsImageReaderFactory* factory) {
    assert((factory != nullptr && sFactory == nullptr) ||
           (factory == nullptr && sFactory != nullptr));
    sFactory = factory;
}

OsImageReaderFactory* getImageReaderFactory() {
    assert(sFactory != nullptr);
    return sFactory;
}

}  // namespace libdbg