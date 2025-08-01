#ifndef __DBG_STACK_TRACE_H__
#define __DBG_STACK_TRACE_H__

#include <sstream>
#include <string>
#include <vector>

#include "dbg_util_def.h"
#include "os_stack_trace.h"
#include "os_symbol_engine.h"
#include "os_thread_manager.h"

namespace libdbg {

/** @brief A fully resolved single stack entry. */
struct DBGUTIL_API StackEntry {
    /** @brief The stack frame index (required if stack trace is partial or reordered). */
    uint32_t m_frameIndex;  // zero means innermost

    /** @brief The frame address. */
    void* m_frameAddress;

    /** @brief Resolved entry debug information. */
    SymbolInfo m_entryInfo;

    StackEntry() : m_frameIndex(0), m_frameAddress(nullptr) {}
    StackEntry(const StackEntry&) = default;
    StackEntry(StackEntry&&) = default;
    StackEntry& operator=(const StackEntry&) = default;
    ~StackEntry() {}
};

/** @typedef A fully resolved stack trace. */
typedef std::vector<StackEntry> StackTrace;

/** @brief Stack entry formatter interface. */
class DBGUTIL_API StackEntryFormatter {
public:
    virtual ~StackEntryFormatter() {}

    /**
     * @brief Formats a stack trace entry
     * @param stackEntry The stack entry.
     * @return std::string The formatted stack entry string.
     */
    virtual std::string formatStackEntry(const StackEntry& stackEntry) = 0;

protected:
    StackEntryFormatter() {}
    StackEntryFormatter(const StackEntryFormatter&) = delete;
    StackEntryFormatter(StackEntryFormatter&&) = delete;
    StackEntryFormatter& operator=(StackEntryFormatter&) = delete;
};

/** @brief Default formatter implementation. */
class DBGUTIL_API DefaultStackEntryFormatter : public StackEntryFormatter {
public:
    DefaultStackEntryFormatter() {}
    DefaultStackEntryFormatter(const DefaultStackEntryFormatter&) = delete;
    DefaultStackEntryFormatter(DefaultStackEntryFormatter&&) = delete;
    DefaultStackEntryFormatter& operator=(DefaultStackEntryFormatter&) = delete;
    ~DefaultStackEntryFormatter() override {}

    std::string formatStackEntry(const StackEntry& stackEntry) override;
};

/** @brief Stack entry printer interface. */
class DBGUTIL_API StackEntryPrinter {
public:
    virtual ~StackEntryPrinter() {}

    virtual void onBeginStackTrace(os_thread_id_t threadId) = 0;
    virtual void onEndStackTrace() = 0;
    virtual void onStackEntry(const char* stackEntry) = 0;

protected:
    StackEntryPrinter() {}
    StackEntryPrinter(const StackEntryPrinter&) = delete;
    StackEntryPrinter(StackEntryPrinter&&) = delete;
    StackEntryPrinter& operator=(StackEntryPrinter&) = delete;
};

/** @brief Stack entry printer that does nothing. */
class DBGUTIL_API NullEntryPrinter : public StackEntryPrinter {
public:
    NullEntryPrinter() {}
    NullEntryPrinter(const NullEntryPrinter&) = delete;
    NullEntryPrinter(NullEntryPrinter&&) = delete;
    NullEntryPrinter& operator=(NullEntryPrinter&) = delete;
    ~NullEntryPrinter() override {}

    void onBeginStackTrace(os_thread_id_t /* threadId */) final {}
    void onEndStackTrace() final {}
    void onStackEntry(const char* /* stackEntry */) final {}
};

/** @brief stack entry printer to a file. */
class DBGUTIL_API FileStackEntryPrinter : public StackEntryPrinter {
public:
    FileStackEntryPrinter(FILE* fileHandle) : m_fileHandle(fileHandle) {}
    FileStackEntryPrinter(const FileStackEntryPrinter&) = delete;
    FileStackEntryPrinter(FileStackEntryPrinter&&) = delete;
    FileStackEntryPrinter& operator=(FileStackEntryPrinter&) = delete;
    ~FileStackEntryPrinter() override {}

    void onBeginStackTrace(os_thread_id_t threadId) override {
        fprintf(m_fileHandle, "[Thread %" PRItid " stack trace]\n", threadId);
    }
    void onEndStackTrace() override {}
    void onStackEntry(const char* stackEntry) { fprintf(m_fileHandle, "%s\n", stackEntry); }

private:
    FILE* m_fileHandle;
};

/** @brief stack entry printer to standard error stream. */
class DBGUTIL_API StderrStackEntryPrinter : public FileStackEntryPrinter {
public:
    StderrStackEntryPrinter() : FileStackEntryPrinter(stderr) {};
    StderrStackEntryPrinter(const StderrStackEntryPrinter&) = delete;
    StderrStackEntryPrinter(StderrStackEntryPrinter&&) = delete;
    StderrStackEntryPrinter& operator=(StderrStackEntryPrinter&) = delete;
    ~StderrStackEntryPrinter() override {}
};

/** @brief stack entry printer to standard output stream. */
class DBGUTIL_API StdoutStackEntryPrinter : public FileStackEntryPrinter {
public:
    StdoutStackEntryPrinter() : FileStackEntryPrinter(stdout) {};
    StdoutStackEntryPrinter(const StdoutStackEntryPrinter&) = delete;
    StdoutStackEntryPrinter(StdoutStackEntryPrinter&&) = delete;
    StdoutStackEntryPrinter& operator=(StdoutStackEntryPrinter&) = delete;
    ~StdoutStackEntryPrinter() override {}
};

#if 0
// TODO: move this to elog after migration is done
/** @brief Stack entry printer to log. */
class DBGUTIL_API LogStackEntryPrinter : public StackEntryPrinter {
public:
    LogStackEntryPrinter(ELogLevel logLevel, const char* title)
        : m_logLevel(logLevel), m_title(title) {}
    void onBeginStackTrace(os_thread_id_t threadId) override {
        ELOG_BEGIN(m_logLevel, "%s:\n[Thread %" PRItid " stack trace]", m_title.c_str(), threadId);
    }
    void onEndStackTrace() override { ELOG_END(); }
    void onStackEntry(const char* stackEntry) { ELOG_APPEND("%s\n", stackEntry); }

private:
    std::string m_title;
    ELogLevel m_logLevel;
};
#endif

/** @brief Stack entry printer to string. */
class DBGUTIL_API StringStackEntryPrinter : public StackEntryPrinter {
public:
    StringStackEntryPrinter() {}
    StringStackEntryPrinter(const StringStackEntryPrinter&) = delete;
    StringStackEntryPrinter(StringStackEntryPrinter&&) = delete;
    StringStackEntryPrinter& operator=(StringStackEntryPrinter&) = delete;
    ~StringStackEntryPrinter() override {}

    void onBeginStackTrace(os_thread_id_t threadId) override {
        m_s << "[Thread " << threadId << " stack trace]" << std::endl;
    }
    void onEndStackTrace() override {}
    void onStackEntry(const char* stackEntry) { m_s << stackEntry << std::endl; }
    std::string getStackTrace() { return m_s.str(); }

private:
    std::stringstream m_s;
};

/** @brief Stack entry printer to multiple printers. */
class DBGUTIL_API MultiStackEntryPrinter : public StackEntryPrinter {
public:
    MultiStackEntryPrinter(StackEntryPrinter* printer1, StackEntryPrinter* printer2) {
        m_printers.push_back(printer1);
        m_printers.push_back(printer2);
    }
    MultiStackEntryPrinter(const MultiStackEntryPrinter&) = delete;
    MultiStackEntryPrinter(MultiStackEntryPrinter&&) = delete;
    MultiStackEntryPrinter& operator=(MultiStackEntryPrinter&) = delete;
    ~MultiStackEntryPrinter() override {}

    inline void addPrinter(StackEntryPrinter* printer) { m_printers.push_back(printer); }

    void onBeginStackTrace(os_thread_id_t threadId) override {
        for (StackEntryPrinter* printer : m_printers) {
            printer->onBeginStackTrace(threadId);
        }
    }

    void onEndStackTrace() override {
        for (StackEntryPrinter* printer : m_printers) {
            printer->onEndStackTrace();
        }
    }

    void onStackEntry(const char* stackEntry) {
        for (StackEntryPrinter* printer : m_printers) {
            printer->onStackEntry(stackEntry);
        }
    }

private:
    std::vector<StackEntryPrinter*> m_printers;
};

/**
 * @brief Retrieves raw stack trace of a thread by context. Context is either captured by calling
 * thread, or is passed by OS through an exception/signal handler.
 * @param[out] stackTrace The resulting stack trace.
 * @param context[opt] OS-specific thread context. Pass null to print current thread call stack.
 * @return LibDbgErr The operation result.
 */
inline LibDbgErr getRawStackTrace(RawStackTrace& stackTrace, void* context = nullptr) {
    return getStackTraceProvider()->getStackTrace(context, stackTrace);
}

/**
 * @brief Converts raw stack frames to resolved stack frames.
 * @param rawStackTrace The raw stack trace.
 * @param[out] stackTrace The resulting resolved stack trace.
 * @return LibDbgErr The operation result.
 */
extern DBGUTIL_API LibDbgErr resolveRawStackTrace(RawStackTrace& rawStackTrace,
                                                  StackTrace& stackTrace);

/**
 * @brief Retrieves a fully resolved stack trace of a thread by an optional context. Context is
 * either captured by calling thread, or is passed by OS through an exception/signal handler.
 * @param[out] stackTrace The resulting resolved stack trace.
 * @param context[opt] OS-specific thread context. Pass null to capture current thread call stack.
 * @return LibDbgErr The operation result.
 */
inline LibDbgErr getStackTrace(StackTrace& stackTrace, void* context = nullptr) {
    RawStackTrace rawStackTrace;
    LibDbgErr err = getRawStackTrace(rawStackTrace, context);
    if (err != LIBDBG_ERR_OK) {
        return err;
    }
    return resolveRawStackTrace(rawStackTrace, stackTrace);
}

/**
 * @brief Converts raw stack frames to resolved stack frames in string form.
 * @param stackTrace The raw stack trace.
 * @param skip[opt] The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 * @param threadId[opt] Optional thread id.
 * @return std::string The resulting resolved stack trace string.
 */
extern DBGUTIL_API std::string rawStackTraceToString(const RawStackTrace& stackTrace, int skip = 0,
                                                     StackEntryFormatter* formatter = nullptr,
                                                     os_thread_id_t threadId = 0);

/**
 * @brief Converts resolved stack frames to string form.
 * @param stackTrace The stack trace.
 * @param skip[opt] The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 * @param threadId[opt] Optional thread id (for printing purposes only). If not specified, current
 * thread id will be used.
 * @return std::string The resulting resolved stack trace string.
 */
extern DBGUTIL_API std::string stackTraceToString(const StackTrace& stackTrace, int skip = 0,
                                                  StackEntryFormatter* formatter = nullptr,
                                                  os_thread_id_t threadId = 0);

/**
 * @brief Prints stack trace by a given context. Context is either captured by calling thread, or is
 * passed by OS through an exception/signal handler.
 * @param context[opt] OS-specific thread context. Pass null to print current thread call stack.
 * @param skip[opt] The number of frames to skip.
 * @param printer[opt] Stack entry printer. Pass null to print to standard error stream.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
extern DBGUTIL_API void printStackTraceContext(void* context = nullptr, int skip = 0,
                                               StackEntryPrinter* printer = nullptr,
                                               StackEntryFormatter* formatter = nullptr);

/**
 * @brief Prints current stack trace. If no argument is passed, then the stack trace is printed to
 * the standard error stream, using default stack entry formatting.
 * @param skip[opt] The number of frames to skip.
 * @param printer[opt] Stack entry printer. Pass null to print to standard error stream.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
inline void printStackTrace(int skip = 0, StackEntryPrinter* printer = nullptr,
                            StackEntryFormatter* formatter = nullptr) {
    printStackTraceContext(nullptr, skip, printer, formatter);
}

/**
 * @brief Dumps stack trace to standard error stream.
 * @param skip[opt] The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
inline void dumpStackTrace(int skip = 0, StackEntryFormatter* formatter = nullptr) {
    printStackTrace(skip, nullptr, formatter);
}

/**
 * @brief Dumps stack trace from context to standard error stream. Context is either captured by
 * calling thread, or is passed by OS through an exception/signal handler.
 * @param context[opt] OS-specific thread context. Pass null to dump current thread call stack.
 * @param skip[opt] The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
inline void dumpStackTraceContext(void* context, int skip = 0,
                                  StackEntryFormatter* formatter = nullptr) {
    printStackTraceContext(context, skip, nullptr, formatter);
}

#if 0
/**
 * @brief Prints stack trace to log with the given log level.
 * @param logLevel The log level.
 * @param title The title to print (will be followed by a colon).
 * @param skip The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
inline void logStackTrace(ELogLevel logLevel, const char* title, int skip,
                          StackEntryFormatter* formatter = nullptr) {
    LogStackEntryPrinter printer(logLevel, title);
    printStackTrace(skip, &printer, formatter);
}

/**
 * @brief Prints stack trace to log with the given log level. Context is either captured by calling
 * thread, or is passed by OS through an exception/signal handler.
 * @param context[opt] OS-specific thread context. Pass null to log current thread call stack.
 * @param logLevel The log level.
 * @param title The title to print (will be followed by a colon).
 * @param skip The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
inline void logStackTraceContext(void* context, ELogLevel logLevel, const char* title, int skip,
                                 StackEntryFormatter* formatter = nullptr) {
    LogStackEntryPrinter printer(logLevel, title);
    printStackTraceContext(context, skip, &printer, formatter);
}
#endif

/**
 * @brief Formats stack trace to string.
 * @param skip The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 * @return std::string The resulting resolved stack trace string.
 */
inline std::string getStackTraceString(int skip = 0, StackEntryFormatter* formatter = nullptr) {
    StringStackEntryPrinter printer;
    printStackTrace(skip, &printer, formatter);
    return printer.getStackTrace();
}

/**
 * @brief Formats stack trace from context to string. Context is either captured by calling thread,
 * or is passed by OS through an exception/signal handler.
 * @param context[opt] OS-specific thread context. Pass null to print current thread call stack.
 * @param skip The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 * @return std::string The resulting resolved stack trace string.
 */
inline std::string stackTraceContextToString(void* context, int skip,
                                             StackEntryFormatter* formatter = nullptr) {
    StringStackEntryPrinter printer;
    printStackTraceContext(context, skip, &printer, formatter);
    return printer.getStackTrace();
}

/** @typedef Raw stack trace of all threads. */
typedef std::vector<std::pair<os_thread_id_t, RawStackTrace>> AppRawStackTrace;

/**
 * @brief Retrieves raw stack trace of all currently running threads in the application.
 * @param[out] appStackTrace The resulting stack traces for all threads.
 */
extern DBGUTIL_API LibDbgErr getAppRawStackTrace(AppRawStackTrace& appStackTrace);

/**
 * @brief Converts application raw stack frames to resolved stack frames in string form.
 * @param appStackTrace The raw stack trace.
 * @param skip[opt] The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 * @return std::string The resulting resolved stack trace string.
 */
extern DBGUTIL_API std::string appRawStackTraceToString(const AppRawStackTrace& appStackTrace,
                                                        int skip = 0,
                                                        StackEntryFormatter* formatter = nullptr);

/**
 * @brief Prints stack trace of all running threads. If no argument is passed, then the stack trace
 * is printed to the standard error stream, using default stack entry formatting.
 * @param skip[opt] The number of frames to skip.
 * @param printer[opt] Stack entry printer. Pass null to print to standard error stream.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
extern DBGUTIL_API void printAppStackTrace(int skip = 0, StackEntryPrinter* printer = nullptr,
                                           StackEntryFormatter* formatter = nullptr);

/**
 * @brief Dumps stack trace of all running threads to error stream.
 * @param skip[opt] The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
inline void dumpAppStackTrace(int skip = 0, StackEntryFormatter* formatter = nullptr) {
    printAppStackTrace(skip, nullptr, formatter);
}

#if 0
/**
 * @brief Prints stack trace of all running threads to log with the given log level.
 * @param logLevel The log level.
 * @param title The title to print (will be followed by a colon).
 * @param skip The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 */
inline void logAppStackTrace(ELogLevel logLevel, const char* title, int skip,
                             StackEntryFormatter* formatter = nullptr) {
    LogStackEntryPrinter printer(logLevel, title);
    printAppStackTrace(skip, &printer, formatter);
}
#endif

/**
 * @brief Formats stack trace of all running threads to string.
 * @param skip The number of frames to skip.
 * @param formatter[opt] Stack entry formatter. Pass null to use default formatting.
 * @return std::string The resulting resolved stack trace string.
 */
inline std::string getAppStackTraceString(int skip, StackEntryFormatter* formatter = nullptr) {
    StringStackEntryPrinter printer;
    printStackTrace(skip, &printer, formatter);
    return printer.getStackTrace();
}

}  // namespace libdbg

#endif  // __DBG_STACK_TRACE_H__