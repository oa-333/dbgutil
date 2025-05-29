#include "dbg_stack_trace.h"

#include <cstring>
#include <iomanip>
#include <sstream>

#include "os_stack_trace.h"
#include "os_symbol_engine.h"
#include "os_thread_manager.h"
#include "os_util.h"
#include "path_parser.h"

namespace dbgutil {

// TODO: consider prettier alignment by aggregating all frames and then deciding alignment for file
// name, but this requires API change
#define SYM_ALIGN 2
#define FILE_ALIGN 40
#define LIB_ALIGN 30

std::string DefaultStackEntryFormatter::formatStackEntry(int frame, void* addr,
                                                         const SymbolInfo& symbolInfo) {
    // format frame address
    std::stringstream s;
    s << std::setw(SYM_ALIGN) << std::right;
    s << frame << "# " << std::hex << addr << " " << std::dec;

    // format function name if available
    s << std::setw(FILE_ALIGN) << std::left;
    if (symbolInfo.m_symbolName.empty()) {
        s << "N/A";
    } else {
        std::stringstream s2;
        // strip parameters if found (more readable)
        std::string::size_type openParenPos = symbolInfo.m_symbolName.find('(');
        if (openParenPos != std::string::npos) {
            s2 << symbolInfo.m_symbolName.substr(0, openParenPos);
        } else {
            s2 << symbolInfo.m_symbolName;
        }
        s2 << "()";

        if (symbolInfo.m_byteOffset != 0) {
            s2 << " +" << symbolInfo.m_byteOffset;
        }

        s << s2.str();
    }

    // format file and line if available
    // s << std::setw(LIB_ALIGN) << std::left;
    if (symbolInfo.m_fileName.empty()) {
        s << " at <N/A> ";
    } else {
        std::stringstream s2;
        std::string fileName;
        if (PathParser::getFileName(symbolInfo.m_fileName.c_str(), fileName) == DBGUTIL_ERR_OK) {
            s2 << " at " << fileName;
        } else {
            s2 << " at " << symbolInfo.m_fileName;
        }
        if (symbolInfo.m_lineNumber != 0) {
            s2 << ":" << symbolInfo.m_lineNumber;
        }

        s << s2.str();
    }

    // format module name
    if (!symbolInfo.m_moduleName.empty()) {
        std::string moduleFileName;
        if (PathParser::getFileName(symbolInfo.m_moduleName.c_str(), moduleFileName) ==
            DBGUTIL_ERR_OK) {
            s << " (" << moduleFileName << ")";
        }
    }

    return s.str();
}

class PrintFrameListener : public StackFrameListener {
public:
    PrintFrameListener(int skip, StackEntryPrinter* printer, StackEntryFormatter* formatter)
        : m_skip(skip), m_printer(printer), m_formatter(formatter), m_frameIndex(0) {}

    ~PrintFrameListener() final {}

    void onStackFrame(void* frameAddress) final {
        // skip required number of frames
        if (m_skip > 0) {
            --m_skip;
            return;
        }

        // get frame debug info
        SymbolInfo symbolInfo;
        (void)getSymbolEngine()->getSymbolInfo(frameAddress, symbolInfo);

        // format stack frame entry string
        std::string entry = m_formatter->formatStackEntry(m_frameIndex++, frameAddress, symbolInfo);

        // print stack entry
        m_printer->onStackEntry(entry.c_str());
    }

private:
    int m_skip;
    StackEntryPrinter* m_printer;
    StackEntryFormatter* m_formatter;
    int m_frameIndex;
};

std::string rawStackTraceToString(RawStackTrace& stackTrace, int skip /* = 0 */,
                                  StackEntryFormatter* formatter /* = nullptr */,
                                  os_thread_id_t threadId /* = 0 */) {
    StringStackEntryPrinter printer;
    DefaultStackEntryFormatter defaultFormatter;
    if (formatter == nullptr) {
        formatter = &defaultFormatter;
    }

    if (threadId == 0) {
        threadId = OsUtil::getCurrentThreadId();
    }
    printer.onBeginStackTrace(threadId);
    PrintFrameListener listener(skip, &printer, formatter);
    for (void* frameAddress : stackTrace) {
        listener.onStackFrame(frameAddress);
    }
    printer.onEndStackTrace();
    return printer.getStackTrace();
}

void printStackTraceContext(void* context, int skip, StackEntryPrinter* printer,
                            StackEntryFormatter* formatter) {
    // setup defaults if needed
    StderrStackEntryPrinter defaultPrinter;
    DefaultStackEntryFormatter defaultFormatter;
    if (printer == nullptr) {
        printer = &defaultPrinter;
    }
    if (formatter == nullptr) {
        formatter = &defaultFormatter;
    }

    printer->onBeginStackTrace(OsUtil::getCurrentThreadId());
    PrintFrameListener listener(skip, printer, formatter);
    getStackTraceProvider()->walkStack(&listener, context);
    printer->onEndStackTrace();
}

DbgUtilErr getAppRawStackTrace(AppRawStackTrace& appStackTrace) {
    // get all thread ids, for each thread, get its stack trace, except for current thread
    class StackTraceCollector : public ThreadListener {
    public:
        StackTraceCollector(AppRawStackTrace& appStackTrace)
            : m_appStackTrace(appStackTrace), m_result(DBGUTIL_ERR_OK) {}

        void onThread(const ThreadId& threadId) final {
            RawStackTrace rawStackTrace;
            DbgUtilErr rc = getStackTraceProvider()->getThreadStackTrace(threadId, rawStackTrace);
            if (rc == DBGUTIL_ERR_OK) {
                m_appStackTrace.push_back(std::make_pair(threadId, rawStackTrace));
            } else if (m_result == DBGUTIL_ERR_OK) {
                // remember first error only, but continue
                m_result = rc;
            }
        }

        inline DbgUtilErr getResult() const { return m_result; }

    private:
        AppRawStackTrace& m_appStackTrace;
        DbgUtilErr m_result;
    };
    StackTraceCollector collector(appStackTrace);
    DbgUtilErr rc = getThreadManager()->visitThreads(&collector);
    if (rc == DBGUTIL_ERR_OK) {
        rc = collector.getResult();
    }
    return rc;
}

std::string appRawStackTraceToString(AppRawStackTrace& appStackTrace, int skip /* = 0 */,
                                     StackEntryFormatter* formatter /* = nullptr */) {
    std::stringstream res;
    for (auto& stackTrace : appStackTrace) {
        res << rawStackTraceToString(stackTrace.second, skip, formatter,
                                     stackTrace.first.m_osThreadId);
        res << std::endl;
    }
    return res.str();
}

void printAppStackTrace(int skip /* = 0 */, StackEntryPrinter* printer /* = nullptr */,
                        StackEntryFormatter* formatter /* = nullptr */) {
    AppRawStackTrace appStackTrace;
    if (getAppRawStackTrace(appStackTrace) == DBGUTIL_ERR_OK) {
        // setup defaults if needed
        StderrStackEntryPrinter defaultPrinter;
        DefaultStackEntryFormatter defaultFormatter;
        if (printer == nullptr) {
            printer = &defaultPrinter;
        }
        if (formatter == nullptr) {
            formatter = &defaultFormatter;
        }

        for (auto& stackTrace : appStackTrace) {
            printer->onBeginStackTrace(stackTrace.first.m_osThreadId);
            PrintFrameListener listener(skip, printer, formatter);
            for (void* frame : stackTrace.second) {
                listener.onStackFrame(frame);
            }
            printer->onEndStackTrace();
        }
    }
}

}  // namespace dbgutil
