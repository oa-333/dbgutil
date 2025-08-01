# dbgutil Library

Dbgutil is a simple library for common software debug utilities in C++,  
with special focus on stack traces including file and line information.  
Supported platforms are:

- Windows
- Linux
- MinGW

## Description

The dbgutil library allows:

- Retrieving stack traces (raw or with full symbol information)
- Resolving symbol information (with file and line, on Windows/Linux/MinGW)
- Catching fatal exceptions and receiving elaborate exception information
- Querying loaded module information

## Getting Started

In order to use the library, first include the main header "dbg_util.h", and initialize the library:

    #include "dbg_util.h"

    // initialize the library
    DbgUtilErr res = libdbg::initLibDbg();
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

    // do application stuff
    // dump stack trace
    libdbg::dumpStackTrace();

    res = libdbg::termLibDbg();
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

Sample output (MinGW):

    [Thread 45252 stack trace]
     0# 0x7ffe5fc32597 libdbg::printStackTraceContext() +167   at dbg_stack_trace.cpp:185 (libdbgutil.dll)
     1# 0x7ff7f841b050 libdbg::printStackTrace() +48           at dbg_stack_trace.h:238 (wal_test_mingw.exe)
     2# 0x7ff7f841b015 libdbg::dumpStackTrace() +37            at dbg_stack_trace.h:247 (wal_test_mingw.exe)
     3# 0x7ff7f84130ff initRecovery() +1275                     at waL_test.cpp:304 (wal_test_mingw.exe)
     4# 0x7ff7f8412064 testLoadWALRecord() +122                 at waL_test.cpp:122 (wal_test_mingw.exe)
     5# 0x7ff7f8411ebb runTest() +47                            at waL_test.cpp:95 (wal_test_mingw.exe)
     6# 0x7ff7f8411e31 main() +398                              at waL_test.cpp:74 (wal_test_mingw.exe)
     7# 0x7ff7f84112ee __tmainCRTStartup() +366                 at crtexe.c:268 (wal_test_mingw.exe)
     8# 0x7ff7f8411406 mainCRTStartup() +22                     at crtexe.c:190 (wal_test_mingw.exe)
     9# 0x7fff0a9be8d7 BaseThreadInitThunk()                    at <N/A>  (KERNEL32.DLL)
    10# 0x7fff0bf3c34c RtlUserThreadStart()                     at <N/A>  (ntdll.dll)

It is possible to register a log handler (so that internal error messages and application exception information  
can be printed properly), and an exception handler that receives notifications about fatal faults  
(e.g. segmentation fault).

Here is a sample output of the exception handler (Linux):

    2025-07-17 11:01:52.678 FATAL  [35850] dbgutil.linux_exception_handler Received signal 11: Segmentation fault
    Faulting address: (nil)
    Extended exception information: Address not mapped to object

    2025-07-17 11:01:52.678 FATAL  [35850] dbgutil.linux_exception_handler [Thread 8c0a stack trace]
     0# 0x77fb9a36b0b1 libdbg::printStackTraceContext() +186   at dbg_stack_trace.cpp:193 (libdbgutil.so)
     1# 0x77fb9a39f5be libdbg::OsExceptionHandler::prepareCallStack() +74 at os_exception_handler.cpp:79 (libdbgutil.so)
     2# 0x77fb9a390458 libdbg::LinuxExceptionHandler::finalizeSignalHandling() +66 at linux_exception_handler.cpp:224 (libdbgutil.so)
     3# 0x77fb9a3903f5 libdbg::LinuxExceptionHandler::signalHandler() +307 at linux_exception_handler.cpp:214 (libdbgutil.so)
     4# 0x77fb9a3902bf libdbg::LinuxExceptionHandler::signalHandlerStatic() +55 at linux_exception_handler.cpp:191 (libdbgutil.so)
     5# 0x77fb99c45330 N/A                                      at <N/A>  (libc.so.6)
     6# 0x58025499fca6 runSingleThreadedTest() +433             at elog_bench.cpp:1310 (elog_bench)
     7# 0x5802549a2ed4 testPerfSTQuantumCount4096() +131        at elog_bench.cpp:2084 (elog_bench)
     8# 0x5802549a2527 testPerfAllSingleThread() +754           at elog_bench.cpp:1808 (elog_bench)
     9# 0x58025499faee testException() +27                      at elog_bench.cpp:1282 (elog_bench)
    10# 0x58025499e77a main() +305                              at elog_bench.cpp:751 (elog_bench)
    11# 0x77fb99c2a1ca N/A                                      at <N/A>  (libc.so.6)
    12# 0x77fb99c2a28b __libc_start_main()                      at <N/A>  (libc.so.6)
    13# 0x58025499d7c5 _start() +37                             at <N/A>  (elog_bench)

    2025-07-17 11:01:52.678 FATAL  [35850] dbgutil.linux_exception_handler Aborting after fatal exception, see details above.
    Aborted


### Dependencies & Limitations

The dbgutil package depends on libunwind on Linux/MinGW, and dbghelp.dll on Windows.  
The supported debug format on Linux/MinGW systems is DWARF 5,  
Any toolchain that produces ELF or PE32 binary image with DWARF 5 (or pdb) debug information is a possible candidate for usage with dbgutil. On platforms/toolchains without explicit support, a few compile time preprocessor definitions may be added in order to enable such support.

Feature/pull requests and bug reports are welcome.

### Installing

The library can be built and installed by running:

    build.sh --install-dir <install-path>
    build.bat --install-dir <install-path>

(Checkout the possible options with --help switch).

Add to compiler include path:

    -I<install-path>/include
    
Add to linker flags:

    -L<install-path>/lib -ldbgutil

For CMake builds it is possible to use FetchContent as follows:

    FetchContent_Declare(dbgutil
        GIT_REPOSITORY https://github.com/oa-333/dbgutil.git
        GIT_TAG v0.1.1
    )
    FetchContent_MakeAvailable(dbgutil)
    target_include_directories(
        <your project name here>
        PRIVATE
        ${dbgutil_SOURCE_DIR}/inc
    )
    target_link_libraries(<your project name here> dbgutil)

dbgutil supports C++ standard version 11 and above. If your project requires a higher version, make sure to define CMAKE_CXX_STANDARD accordingly before including dbgutil with FetchContent_Declare().

In the future it may be uploaded to package managers (e.g. vcpkg).

## Help

See [examples](#examples) section below, and documentation in header files for more information.

## Authors

Oren A. (oa.github.333@gmail.com)

## License

This project is licensed under the Apache 2.0 License - see the LICENSE file for details.

## Examples

### Playing with Stack Traces

Raw stack trace (frame addresses only), can be achieved as follows:

    #include "dbg_stack_trace.h"

    DbgUtilErr res = libdbg::RawStackTrace rawStackTrace;
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

In order to resolve all stack frames (i.e. get symbol info, file, line, module, etc.), do this:

    libdbg::StackTrace stackTrace;
    res = libdbg::resolveRawStackTrace(rawStackTrace, stackTrace);
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

The separation was done in order to allow retrieving a call stack quickly,  
and doing symbol resolving some time later on the background.  

In order to simply get the fully resolved current stack trace, do the following:

    std::string stackTraceStr = libdbg::getStackTraceString();

Here is sample output (on Linux machine):

    2025-07-07 09:53:14.816 INFO   [46748] {main} <elog_root> [Thread 46748 (0xb69c) <main> stack trace]
 
    0# 0x716a4879e618 libdbg::printStackTraceContext() +185   at dbg_stack_trace.cpp:185 (libdbgutil.so)
    1# 0x716a4903b8cf libdbg::printStackTrace() +46           at dbg_stack_trace.h:238 (libelog.so)
    2# 0x716a4903b1c1 elog::ELogSystem::logStackTrace() +101   at elog_system.cpp:1527 (libelog.so)
    3# 0x5c4bddcbfb30 initRecovery() +973                      at waL_test.cpp:299 (wal_test_linux)
    4# 0x5c4bddcbeb04 testLoadWALRecord() +120                 at waL_test.cpp:122 (wal_test_linux)
    5# 0x5c4bddcbe96d runTest() +51                            at waL_test.cpp:95 (wal_test_linux)
    6# 0x5c4bddcbe8da main() +389                              at waL_test.cpp:74 (wal_test_linux)
    7# 0x716a4842a1ca N/A                                      at <N/A>  (libc.so.6)
    8# 0x716a4842a28b __libc_start_main()                      at <N/A>  (libc.so.6)
    9# 0x5c4bddcba745 _start() +37                             at <N/A>  (wal_test_linux)

Skipping frames is supported for cleaner output, as well as custom formatting.  
Checkout the API at dbg_stack_trace.h for more details.

If you would like to dump the current stack trace to the standard error stream, it can be done like this:

    libdbg::dumpStackTrace();

### Dumping pstack-like application stack trace of all threads

Occasionally, it may be desired to dump stack trace of all active threads. It may be achieved like this:

    libdbg::dumpAppStackTrace().

This is an experimental feature that may be susceptible to deadlocks, so it is advised to use this feature  
with caution, perhaps only as a last resort before crashing.  

One might wonder why there is a need for such a full stack trace dump, since a core file already contains this information, but there are some cases of very restriction production environments, where dumping a core file is not allowed or not possible. In these cases, this feature is very handy.

It is also possible to retrieve this dump as a string so it can be sent to a log file:

    libdbg::getAppStackTraceString();

### Exception Handling

In order to enable exception handling, the dbgutil must be initialized with at least DBGUTIL_CATCH_EXCEPTIONS flag enabled:

    libdbg::initLibDbg(nullptr, nullptr, libdbg::LS_FATAL, DBGUTIL_CATCH_EXCEPTIONS);

By default, dbgutil catches the following signals on Linux and MinGW:

- SIGSEGV
- SIGILL
- SIGFPE
- SIGBUS (not available on MinGW)
- SIGTRAP (not available on MinGW)

On Windows, an unhandled exception filter is registered to catch all critical exceptions.

### std::terminate() Handling

In addition, dbgutil can be ordered to catch std::terminate() as well:

    libdbg::initLibDbg(nullptr, nullptr, libdbg::LS_FATAL, DBGUTIL_CATCH_EXCEPTIONS | DBGUTIL_SET_TERMINATE_HANDLER);

### Sending Exception Report To Log

If all that is required is to write exception information to log, then adding the DBGUTIL_LOG_EXCEPTIONS suffices:

    libdbg::initLibDbg(nullptr, nullptr, LS_FATAL, DBGUTIL_CATCH_EXCEPTIONS | DBGUTIL_LOG_EXCEPTIONS);

This will have all exception information sent to the installed [log handler](#log-handling).

### Registering Custom Exception Listener

It is possible to register an exception listener, that will receive notifications about fatal events.  
Currently only fatal exceptions, and std::terminate() are reported.  
In order to register such a listener, first derive from OsExceptionListener, and then pass a pointer to initLibDbg():

    class MyExceptionListener : public libdbg::OsExceptionListener {
        // implement pure virtual functions
    };

    static MyExceptionListener myExceptionListener;

    libdbg::initLibDbg(&myExceptionListener, nullptr, LS_FATAL, 
        DBGUTIL_CATCH_EXCEPTIONS | DBGUTIL_SET_TERMINATE_HANDLER);

### Generating Core Dumps on Windows/Linux during Crash Handling

It is possible to order dbgutil to generate core dump file before crashing.  

To enable this option, dbgutil should be initialized with the DBGUTIL_EXCEPTION_DUMP_CORE flag:

    libdbg::initLibDbg(null, nullptr, LS_FATAL, DBGUTIL_EXCEPTION_DUMP_CORE);
    
On Windows this will have dbgutil generate a mini-dump file.  
Be advised that having a process generate its own mini-dump is not advised according to MSDN documentation,  
so consider this is a best effort attempt.

### Combining All Options

If all exception options are to be used, then this form can be used instead:

    libdbg::initLibDbg(&myExceptionListener, nullptr, LS_FATAL, DBGUTIL_FLAGS_ALL);

### Exception Handling Sequence

When an exception occurs, and the user configured dbgutil to catch exceptions, the following takes place:

- If user configured exception logging, then:
    - An elaborate error message is printed to log, describing the signal/exception details
    - Full stack trace, with file and line information is also written to log
- If user installed an exception listener, then a full exception information object is dispatched to the registered listener
- On Windows, if user configured to do so, an attempt is made to generate mini-dump
- On Linux the default core dump generation takes place without any intervention required

### Log Handling

In order to receive exception messages, as well internal errors or traces, a log handler should be installed.  
A default log handler that prints to the standard error stream can be used as follows:

    // initialize the library
    DbgUtilErr res = libdbg::initLibDbg(DBGUTIL_DEFAULT_LOG_HANDLER, LS_FATAL);
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

It is possible to derive from LogHandler, and redirect dbgutil log messages into the application's  
standard logging system:



    class MyLogHandler : public libdbg::LogHandler {
    public:
        libdbg::LogSeverity onRegisterLogger(libdbg::LogSeverity severity, const char* loggerName,
                                              size_t loggerId) final {
            // TODO: handle event - logger initializing
            // if no special handling is required then don't override this method
            // it may be handing here to map dbgutil logger to application logger
        }

        void onUnregisterLogger(size_t loggerId) final {
            // TODO: handle event - logger terminating
            // if no special handling is required then don't override this method
        }

        void onMsg(libdbg::LogSeverity severity, size_t loggerId, const char* loggerName,
                   const char* msg) final {
            // TODO: redirect message to internal logging system, logger id may be used to 
            // locate mapped logger from onRegisterLogger()
        }
    };

    MyLogHandler logHandler;

    // initialize the library
    // pass custom log handler
    // receive messages with INFO and higher log level
    // receive exception log messages
    // no exception listener used
    DbgUtilErr res = libdbg::initLibDbg(nullptr, &logHandler, libdbg::LS_INFO, DBGUTIL_LOG_EXCEPTIONS);
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

### Traverse All Threads

It is possible to get a list of the identifiers of all active threads, as follows:

    #include "os_thread_manager.h"

    libdbg::visitThreads([](libdbg::os_thread_id_t threadId){
        printf("Got thread %" PRItid, threadId);
    });

Pay attention that on non-Windows platforms, the given thread identifier is a system identifier,  
and not the pthread_t handle.

Also note the PRItid format specification that is defined properly per platform.

### Retrieving Symbol Information

It is possible to directly retrieve the debug symbol information for a given address:

    void* symAddress = ...;
    libdbg::SymbolInfo symInfo;
    DbgUtilErr rc = getSymbolEngine()->getSymbolInfo(symAddress, symInfo);
    if (rc == DBGUTIL_ERR_OK) {
        // do something with symbol info
    }

The SymbolInfo struct contains the following information:

- Containing module (base address, name)
- Symbol name (demangled)
- Symbol source location (file, line, possibly column)
- Symbol memory location (start address of symbol, offset of given address within symbol bounds)

Normally, this API should be used for locating function information and not for data.