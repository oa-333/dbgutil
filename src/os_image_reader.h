#ifndef __OS_IMAGE_READER_H__
#define __OS_IMAGE_READER_H__

#include <unordered_map>
#include <vector>

#include "buffered_file_reader.h"
#include "dbgutil_common.h"

namespace dbgutil {

/** @brief A platform independent section definition in a binary image. */
struct OsImageSection {
    /** @var The name of the section. */
    std::string m_name;

    /** @var Offset within image file. */
    uint64_t m_offset;

    /** @var Size in bytes of the section. */
    uint64_t m_size;

    /** @var Pointer to the start of the section (non-null only if materialized). */
    char* m_start;
};

/** @brief Platform-independent parent class for reading an binary image file. */
class OsImageReader {
public:
    /** @brief Constructor. */
    OsImageReader();

    /** @brief Destructor. */
    virtual ~OsImageReader() {}

    static void initLogger();
    static void termLogger();

    /**
     * @brief Opens the binary image file for reading.
     * @param path The path to the image file on the file system.
     * @param moduleBase The base address of the corresponding loaded module in memory (may be
     * required for address relocation computations).
     * @return DbgUtilErr The operation result.
     */
    virtual DbgUtilErr open(const char* path, void* moduleBase);

    /** @brief Closes the binary image file. */
    virtual void close();

    /**
     * @brief Searches for a symbol in the binary image file's symbol table (provided by linker).
     * @param symbolAddress The symbol address to search.
     * @param[out] symbolName The name of the resulting symbol (if symbol was found).
     * @param[out] fileName The file containing the symbol (if symbol was found).
     * @param[out] address The actual start address of the symbol.
     * @return DbgUtilErr The operation result.
     */
    virtual DbgUtilErr searchSymbol(void* symAddress, std::string& symName, std::string& fileName,
                                    void** address);

    /**
     * @brief Retrieves a section by name.
     * @param name The name of the section to search.
     * @param[out] section The resulting section (if found).
     * @return DbgUtilErr The operation result.
     */
    DbgUtilErr getSection(const char* name, OsImageSection& section);

    /**
     * @brief Retrieves sections by name prefix.
     * @param prefix The section name prefix to search.
     * @param[out] sections The resulting sections found.
     */
    DbgUtilErr getSections(const char* prefix, std::vector<OsImageSection>& sections);

    /**
     * @brief Visits sections, optionally filtering by a prefix.
     * @tparam F The visitor function type. Expected signature is: "bool f(const OsImageSection&)".
     * @param prefix The prefix to search. Pass null or empty string to avoid filtering.
     * @param f The visitor function.
     */
    template <typename F>
    inline DbgUtilErr forEachSection(const char* prefix, F f) {
        std::vector<OsImageSection> sections;
        DbgUtilErr rc = getSections(prefix, sections);
        if (rc != DBGUTIL_ERR_OK) {
            return rc;
        }
        for (const OsImageSection& section : sections) {
            if (!f(section)) {
                break;
            }
        }
        return DBGUTIL_ERR_OK;
    }

    /** @brief Queries whether this is a 64 bit binary image. */
    inline bool getIs64Bit() const { return m_is64Bit; }

    /** @brief Queries whether this is an executable image or a shared object image. */
    inline bool getIsExe() const { return m_isExe; }

    /** @brief Returns a relocation base suggested by the binary image. */
    inline uint64_t getRelocationBase() { return m_relocBase; }

protected:
    /** @brief Implement image reading. */
    virtual DbgUtilErr readImage() = 0;

    /** @brief Reset object state. */
    virtual void resetData() {}

    BufferedFileReader m_fileReader;
    std::string m_imagePath;
    uint64_t m_fileSizeBytes;
    // the module base as loaded by process in memory
    char* m_moduleBase;
    bool m_is64Bit;
    bool m_isExe;
    // the image base as appears on image file
    uint64_t m_relocBase;

    // we build an interval map of symbol offsets pointing to names, ordered by interval start
    // all intervals are not overlapping
    struct OsSymbolInfo {
        /** @var Offset of symbol relative to start of image. */
        uint64_t m_offset;

        /** @var Size of symbol in bytes. */
        uint64_t m_size;

        /** @var The name of the symbol. */
        std::string m_name;

        /** @var The index of the file containing the symbol. */
        uint32_t m_srcFileIndex;

        /** @brief The index of the section containing the symbol. */
        int m_originSectionIndex;

        /** @brief Comparator for ordering symbols by offset. */
        inline bool operator<(const OsSymbolInfo& symInfo) const {
            return m_offset < symInfo.m_offset;
        }

        /** @brief Comparator for searching by offset. */
        inline bool operator<(uint64_t symOffset) const { return m_offset < symOffset; }

        /** @brief Queries whether an offset matches a symbol. */
        inline bool contains(uint64_t symOffset) const {
            return symOffset >= m_offset && symOffset < (m_offset + m_size);
        }
    };

    typedef std::vector<OsSymbolInfo> SymInfoSet;
    SymInfoSet m_symInfoSet;
    std::vector<std::string> m_srcFileNames;

    typedef std::unordered_map<std::string, OsImageSection> OsSectionMap;
    OsSectionMap m_sectionMap;

    typedef std::vector<char> SectionBuffer;
    typedef std::unordered_map<std::string, SectionBuffer> MaterializedSectionMap;
    MaterializedSectionMap m_materializedSectionMap;

    DbgUtilErr materializeSection(OsImageSection& section);
};

/** @brief Abstract factory for creating image readers. */
class OsImageReaderFactory {
public:
    OsImageReaderFactory(const OsImageReaderFactory&) = delete;
    OsImageReaderFactory(OsImageReaderFactory&&) = delete;
    OsImageReaderFactory& operator=(const OsImageReaderFactory&) = delete;
    virtual ~OsImageReaderFactory() {}

    /** @brief Creates the image reader. */
    virtual OsImageReader* createImageReader() = 0;

protected:
    OsImageReaderFactory() {}
};

/** @brief Installs an image reader factory. */
extern void setImageReaderFactory(OsImageReaderFactory* factory);

/** @brief Retrieves the installed image reader factory. */
extern OsImageReaderFactory* getImageReaderFactory();

/** @brief Creates an image reader. */
inline OsImageReader* createImageReader() { return getImageReaderFactory()->createImageReader(); }

}  // namespace dbgutil

#endif  // __OS_IMAGE_READER_H__