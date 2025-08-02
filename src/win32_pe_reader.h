#ifndef __WIN32_PE_READER_H__
#define __WIN32_PE_READER_H__

#include "dbgutil_common.h"
#include "os_image_reader.h"

#ifdef DBGUTIL_WINDOWS

#include <unordered_map>
#include <vector>

namespace dbgutil {

class Win32PEReader : public OsImageReader {
public:
    Win32PEReader();
    ~Win32PEReader() {}

protected:
    DbgUtilErr readImage() final;
    void resetData() final;

private:
    bool m_isPE32Plus;
    std::vector<char> m_strTab;
    // uint64_t m_strTabOffset;
    // char* m_strTab;
    // uint32_t m_strTabSize;
    IMAGE_FILE_HEADER m_fileHeader;

    // maintain section vector for access by index
    struct SectionInfo {
        std::string m_name;
        uint64_t m_fileOffset;
        uint64_t m_virtualOffset;
        uint64_t m_size;
    };
    std::vector<SectionInfo> m_sections;

    // mini-section map (speculative)
    typedef std::unordered_map<uint64_t, uint64_t> FuncSizeMap;
    std::vector<FuncSizeMap> m_miniSections;

    DbgUtilErr scanHdrs32();
    DbgUtilErr scanHdrs64();
    DbgUtilErr readSectionHeaders();
    DbgUtilErr buildSymTab();

    DbgUtilErr getSectionName(unsigned char* nameRef, std::string& name);
    DbgUtilErr getSymbolName(unsigned char* shortName, unsigned long* longName, std::string& name);
    DbgUtilErr getFileName(unsigned char* rawName, std::string& name);
};

extern DbgUtilErr initWin32PEReader();
extern DbgUtilErr termWin32PEReader();

}  // namespace dbgutil

#endif  // DBGUTIL_WINDOWS

#endif  // __WIN32_PE_READER_H__