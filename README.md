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
- Sending life-sign reports to a shared memory segment, for post-mortem crash analysis

## Getting Started

In order to use the library, first include the main header "dbg_util.h", and initialize the library:

    #include "dbg_util.h"

    // initialize the library
    dbgutil::DbgUtilErr res = dbgutil::initDbgUtil();
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

    // do application stuff
    // dump stack trace
    dbgutil::dumpStackTrace();

    res = dbgutil::termDbgUtil();
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

Sample output (MinGW):

    [Thread 45252 stack trace]
     0# 0x7ffe5fc32597 dbgutil::printStackTraceContext() +167   at dbg_stack_trace.cpp:185 (libdbgutil.dll)
     1# 0x7ff7f841b050 dbgutil::printStackTrace() +48           at dbg_stack_trace.h:238 (wal_test_mingw.exe)
     2# 0x7ff7f841b015 dbgutil::dumpStackTrace() +37            at dbg_stack_trace.h:247 (wal_test_mingw.exe)
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
     0# 0x77fb9a36b0b1 dbgutil::printStackTraceContext() +186   at dbg_stack_trace.cpp:193 (libdbgutil.so)
     1# 0x77fb9a39f5be dbgutil::OsExceptionHandler::prepareCallStack() +74 at os_exception_handler.cpp:79 (libdbgutil.so)
     2# 0x77fb9a390458 dbgutil::LinuxExceptionHandler::finalizeSignalHandling() +66 at linux_exception_handler.cpp:224 (libdbgutil.so)
     3# 0x77fb9a3903f5 dbgutil::LinuxExceptionHandler::signalHandler() +307 at linux_exception_handler.cpp:214 (libdbgutil.so)
     4# 0x77fb9a3902bf dbgutil::LinuxExceptionHandler::signalHandlerStatic() +55 at linux_exception_handler.cpp:191 (libdbgutil.so)
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
        GIT_TAG v0.1.2
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

# Documentation

## Contents
- [Stack Traces](#stack-traces)
    - [Playing with Stack Traces](#playing-with-stack-traces)
    - [Dumping pstack-like Stack Trace](#dumping-pstack-like-application-stack-trace-of-all-threads)
- [Exception Handling](#exception-handling)
    - [Enabling Exception Handling](#enabling-exception-handling)
    - [Handling std::terminate()](#handling-stdterminate)
    - [Sending Exception Report To Log](#sending-exception-report-to-log)
    - [Registering Custom Exception Listener](#registering-custom-exception-listener)
    - [Generating Core Dumps](#generating-core-dumps-on-windowslinux-during-crash-handling)
    - [Combining All Options](#combining-all-options)
    - [Exception Handling Sequence](#exception-handling-sequence)
- [Log Handling](#log-handling)
- [Threads](#threads)
    - [Traverse All Thread Ids](#traverse-all-thread-ids)
    - [Executing Asynchronous Thread Request](#executing-asynchronous-requests-on-target-thread-context)
    - [Asynchronous Request Waiting Modes](#asynchronous-request-waiting-modes)
    - [Submitting Asynchronous Requests](#submitting-asynchronous-requests)
    - [Handling Asynchronous Request Deadlocks](#handling-asynchronous-request-deadlocks)
- [Retrieving Symbol Information](#retrieving-symbol-information)
- [Life Sign Management](#life-sign-management)
    - [Initializing The Life-Sign Manager](#initializing-the-life-sign-manager)
    - [Writing Context and Life-Sign Records](#writing-context-and-life-sign-records)
    - [Closing The Life-Sign Manager](#closing-the-life-sign-manager)
    - [Listing Existing Life-Sign Segments](#listing-existing-life-sign-segments)
    - [Inspecting Life-Sign Segments](#inspecting-life-sign-segments)
    - [Keeping Windows Life-Sign Segments In-Sync](#keeping-windows-life-sign-segments-in-sync)
    - [Keeping Windows Life-Sign Segments Alive](#keeping-windows-life-sign-segments-alive)

## Stack Traces

### Playing with Stack Traces

Raw stack trace (frame addresses only), can be achieved as follows:

    #include "dbg_stack_trace.h"

    dbgutil::DbgUtilErr res = dbgutil::RawStackTrace rawStackTrace;
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

In order to resolve all stack frames (i.e. get symbol info, file, line, module, etc.), do this:

    dbgutil::StackTrace stackTrace;
    dbgutil::DbgUtilErr res = dbgutil::resolveRawStackTrace(rawStackTrace, stackTrace);
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

The separation was done in order to allow retrieving a call stack quickly,  
and doing symbol resolving some time later on the background.  

In order to simply get the fully resolved current stack trace, do the following:

    std::string stackTraceStr = dbgutil::getStackTraceString();

Here is sample output (on Linux machine):

    2025-07-07 09:53:14.816 INFO   [46748] {main} <elog_root> [Thread 46748 (0xb69c) <main> stack trace]
 
    0# 0x716a4879e618 dbgutil::printStackTraceContext() +185   at dbg_stack_trace.cpp:185 (libdbgutil.so)
    1# 0x716a4903b8cf dbgutil::printStackTrace() +46           at dbg_stack_trace.h:238 (libelog.so)
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

    dbgutil::dumpStackTrace();

### Dumping pstack-like application stack trace of all threads

Occasionally, it may be desired to dump stack trace of all active threads. It may be achieved like this:

    dbgutil::dumpAppStackTrace().

This is an experimental feature that may be susceptible to deadlocks, so it is advised to use this feature  
with caution, perhaps only as a last resort before crashing.  

One might wonder why there is a need for such a full stack trace dump, since a core file already contains this information, but there are some cases of very restriction production environments, where dumping a core file is not allowed or not possible. In these cases, this feature is very handy.

It is also possible to retrieve this dump as a string so it can be sent to a log file:

    dbgutil::getAppStackTraceString();

## Exception Handling

### Enabling Exception Handling

In order to enable exception handling, the dbgutil must be initialized with at least DBGUTIL_CATCH_EXCEPTIONS flag enabled:

    dbgutil::initDbgUtil(nullptr, nullptr, dbgutil::LS_FATAL, DBGUTIL_CATCH_EXCEPTIONS);

By default, dbgutil catches the following signals on Linux and MinGW:

- SIGSEGV
- SIGILL
- SIGFPE
- SIGBUS (not available on MinGW)
- SIGTRAP (not available on MinGW)

On Windows, an unhandled exception filter is registered to catch all critical exceptions.

### Handling std::terminate()

In addition, dbgutil can be ordered to catch std::terminate() as well:

    dbgutil::initDbgUtil(nullptr, nullptr, dbgutil::LS_FATAL, DBGUTIL_CATCH_EXCEPTIONS | DBGUTIL_SET_TERMINATE_HANDLER);

### Sending Exception Report To Log

If all that is required is to write exception information to log, then adding the DBGUTIL_LOG_EXCEPTIONS suffices:

    dbgutil::initDbgUtil(nullptr, nullptr, dbgutil::LS_FATAL, DBGUTIL_CATCH_EXCEPTIONS | DBGUTIL_LOG_EXCEPTIONS);

This will have all exception information sent to the installed [log handler](#log-handling).

### Registering Custom Exception Listener

It is possible to register an exception listener, that will receive notifications about fatal events.  
Currently only fatal exceptions, and std::terminate() are reported.  
In order to register such a listener, first derive from OsExceptionListener, and then pass a pointer to initDbgUtil():

    class MyExceptionListener : public dbgutil::OsExceptionListener {
        // implement pure virtual functions
    };

    static MyExceptionListener myExceptionListener;

    dbgutil::initDbgUtil(&myExceptionListener, nullptr, dbgutil::LS_FATAL, 
        DBGUTIL_CATCH_EXCEPTIONS | DBGUTIL_SET_TERMINATE_HANDLER);

### Generating Core Dumps on Windows/Linux during Crash Handling

It is possible to order dbgutil to generate core dump file before crashing.  

To enable this option, dbgutil should be initialized with the DBGUTIL_EXCEPTION_DUMP_CORE flag:

    dbgutil::initDbgUtil(nullptr, nullptr, dbgutil::LS_FATAL, DBGUTIL_EXCEPTION_DUMP_CORE);
    
On Windows this will have dbgutil generate a mini-dump file.  
Be advised that having a process generate its own mini-dump is not advised according to MSDN documentation,  
so consider this is a best effort attempt.

### Combining All Options

If all exception options are to be used, then this form can be used instead:

    dbgutil::initDbgUtil(&myExceptionListener, nullptr, dbgutil::LS_FATAL, DBGUTIL_FLAGS_ALL);

### Exception Handling Sequence

When an exception occurs, and the user configured dbgutil to catch exceptions, the following takes place:

- If user configured exception logging, then:
    - An elaborate error message is printed to log, describing the signal/exception details
    - Full stack trace, with file and line information is also written to log
- If user installed an exception listener, then a full exception information object is dispatched to the registered listener
- On Windows, if user configured to do so, an attempt is made to generate mini-dump
- On Linux the default core dump generation takes place without any intervention required

## Log Handling

In order to receive exception messages, as well internal errors or traces, a log handler should be installed.  
A default log handler that prints to the standard error stream can be used as follows:

    // initialize the library
    dbgutil::DbgUtilErr res = dbgutil::initDbgUtil(DBGUTIL_DEFAULT_LOG_HANDLER, dbgutil::LS_FATAL);
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

It is possible to derive from LogHandler, and redirect dbgutil log messages into the application's  
standard logging system:

    class MyLogHandler : public dbgutil::LogHandler {
    public:
        dbgutil::LogSeverity onRegisterLogger(dbgutil::LogSeverity severity, const char* loggerName,
                                              size_t loggerId) final {
            // TODO: handle event - logger initializing
            // if no special handling is required then don't override this method
            // it may be handing here to map dbgutil logger to application logger
        }

        void onUnregisterLogger(size_t loggerId) final {
            // TODO: handle event - logger terminating
            // if no special handling is required then don't override this method
        }

        void onMsg(dbgutil::LogSeverity severity, size_t loggerId, const char* loggerName,
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
    dbgutil::DbgUtilErr res = dbgutil::initDbgUtil(nullptr, &logHandler, dbgutil::LS_INFO, DBGUTIL_LOG_EXCEPTIONS);
    if (res != DBGUTIL_ERR_OK) {
        // handle error
    }

## Threads

### Traverse All Thread Ids

It is possible to get a list of the identifiers of all active threads, as follows:

    #include "os_thread_manager.h"

    dbgutil::visitThreads([](dbgutil::os_thread_id_t threadId){
        printf("Got thread %" PRItid, threadId);
    });

Pay attention that on non-Windows platforms, the given thread identifier is a system identifier,  
and not the pthread_t handle.

Also note the PRItid format specification that is defined properly per platform.

### Executing Asynchronous Requests on Target Thread Context

Although not strictly being a debug utility, the dbgutil library also provides the ability to execute requests on a given thread. This is can be done as follows:

    // derive from ThreadExecutor and implement desired action
    class Executor : public dbgutil::ThreadExecutor {
    public:
        dbgutil::DbgUtilErr execRequest() {
            // TODO: execute request
        }
    };

    // run executor on target thread
    Executor executor;
    dbgutil::DbgUtilErr requestResult = DBGUTIL_ERR_OK;
    dbgutil::DbgUtilErr rc = dbgutil::execThreadRequest(threadId, &executor, requestResult);
    if (rc != DBGUTIL_ERR_OK) {
        fprintf(stderr, "Failed to execute request on thread %" PRItid ": %s\n", threadId, 
            dbgutil::errorToString(rc));
    } else {
        if (requestResult != DBGUTIL_ERR_OK) {
            fprintf(stderr, "Request execution on target thread %" PRItid " ended with error: %s\n", 
                threadId, dbgutil::errorToString(requestResult));
        }
    }

### Asynchronous Request Waiting Modes

Waiting for asynchronous requests can be done in two modes:

- Polling
- Notification via condition variable

The wait mode can be specified through an additional ThreadWaitParams parameter. By default a tight loop is used. This can be modified with the ThreadWaitParams members:

    uint64_t pollIntervalMicros = 500;
    dbgutil::ThreadWaitParams waitParams(dbgutil::ThreadWaitMode::TWM_POLLING, pollIntervalMicros);
    dbgutil::DbgUtilErr rc = dbgutil::execThreadRequest(threadId, &executor, requestResult, waitParams);

For notification through a condition variable, the following should be done:

    dbgutil::ThreadWaitParams waitParams(dbgutil::ThreadWaitMode::TWM_NOTIFY);
    dbgutil::execThreadRequest(threadId, &executor, requestResult, waitParams);

In both cases the wait is executed internally by dbgutil during the call to @ref execThreadRequest().

### Submitting Asynchronous Requests

It is possible to just submit the request to the target thread, and wait for the result at a later point in the future. It can be sone as follows:

    // derive from ThreadExecutor and implement desired action
    class Executor : public dbgutil::ThreadExecutor {
    public:
        dbgutil::DbgUtilErr execRequest() {
            // TODO: execute request
        }
    };

    // submit executor to run on target thread
    Executor executor;
    ThreadRequestFuture* future = nullptr;
    dbgutil::DbgUtilErr rc = dbgutil::submitThreadRequest(threadId, &executor, future);
    if (rc != DBGUTIL_ERR_OK) {
        fprintf(stderr, "Failed to submit request to execute on thread %" PRItid ": %s\n", threadId, 
            dbgutil::errorToString(rc));
    } else {
        // do some application stuff

        // then wait for future to finish
        dbgutil::DbgUtilErr requestResult = future->wait();
        if (requestResult != DBGUTIL_ERR_OK) {
            fprintf(stderr, "Request execution on target thread %" PRItid " ended with error: %s\n", 
                threadId, dbgutil::errorToString(requestResult));
        }

        // cleanup
        future->release();
    }

The waiting mode used by the future is determined by the ThreadWaitParams, as explained [above](#asynchronous-request-waiting-modes).

### Handling Asynchronous Request Deadlocks

Since running asynchronous calls on a target thread involves sending a signal, or asynchronous procedure call (APC) request on Windows platforms, it is quite possible that the target thread is either busy doing some user-space work, or even worse, asleep in a condition that does not allow processing incoming signals/APCs.

In such a case it is required to notify the target thread that it should check for incoming signals/APCs. In case of sleep/wait. the target thread just needs to be interrupted from sleep/wait, and return immediately to sleep/wait, and that would suffice to trigger processing of queues signals/APCs. Other conditions may require other measures.

For this purpose a utility helper interface was added, namely @ref ThreadNotifier, such that the user can derive from it and implement any required logic to notify the target thread it needs to process queued signals/APCs. Since on Windows platforms this problem is especially acute when the target thread waits on std::condition_variable object, a helper class, namely CVThreadNotifier, was added.

The following example may clarify how to use a notifier:

    class Notifier : public dbgutil::ThreadNotifier {
    public:
        void notify() override {
            // TODO: implement logic to wake up target thread
        }
    };

    // run executor on target thread as usual, with notifier passed in wait parameters
    Executor executor;
    dbgutil::DbgUtilErr requestResult = DBGUTIL_ERR_OK;
    dbgutil::ThreadWaitParams waitParams;
    waitParams.m_notifier = notifier;
    dbgutil::DbgUtilErr rc = dbgutil::execThreadRequest(threadId, &executor, requestResult, waitParams);

When submitting a thread request and waiting on a future object, the target thread may be notified by the caller before waiting on the future object. Therefore, in this case, the notifier member of the wait parameters object is ignored. The correct logic is as follows:

    ThreadRequestFuture* future = nullptr;
    dbgutil::DbgUtilErr rc = dbgutil::submitThreadRequest(threadId, &executor, future);
    if (rc != DBGUTIL_ERR_OK) {
        fprintf(stderr, "Failed to submit request to execute on thread %" PRItid ": %s\n", threadId, 
            dbgutil::errorToString(rc));
    } else {
        // TODO: trigger target thread to wake up and process incoming signals/APCs
        
        // do some application stuff

        // then wait for future to finish
        dbgutil::DbgUtilErr requestResult = future->wait();
        if (requestResult != DBGUTIL_ERR_OK) {
            fprintf(stderr, "Request execution on target thread %" PRItid " ended with error: %s\n", 
                threadId, dbgutil::errorToString(requestResult));
        }

        // cleanup
        future->release();
    }

## Retrieving Symbol Information

It is possible to directly retrieve the debug symbol information for a given address:

    void* symAddress = ...;
    dbgutil::SymbolInfo symInfo;
    dbgutil::DbgUtilErr rc = dbgutil::getSymbolEngine()->getSymbolInfo(symAddress, symInfo);
    if (rc == DBGUTIL_ERR_OK) {
        // do something with symbol info
    }

The SymbolInfo struct contains the following information:

- Containing module (base address, name)
- Symbol name (demangled)
- Symbol source location (file, line, possibly column)
- Symbol memory location (start address of symbol, offset of given address within symbol bounds)

Normally, this API should be used for locating function information and not for data.

## Life Sign Management

In order to allow post-mortem crash analysis, dbgutil provides API for the application to occasionally send data to a shared memory segment. This is done by the life-sign manager. This is a rather generic interface, providing the infrastructure, yet placing much responsibility on the application.

### Initializing The Life-Sign Manager

In order to initialize this mechanism, the following needs to be done:

    dbgutil::DbgUtilErr rc = dbgutil::getLifeSignManager()->createLifeSignShmSegment(
        contextAreaSize, lifeSignAreaSize, maxThreads, shareWrite);

The life-sign shared memory segment is divided into two main areas:

- The context area
- The life-sign area

Both areas are organized in records. Usually this should be text, but not necessarily. The application is free to use any format it wishes, as the interpretation of records is the application's responsibility as well.

The context area is designed to be used for storing general data and metadata required for interpreting periodic life-sign record. This could be thread names according to id, for instance, or application version number, etc. The context area is limited in size, and once all area is used up, no more context records can be added.

The life-sign area is designed to be used for periodic ongoing life-sign records. This should include hints as to what happened just right before the application crashed. The life-sign area is divided into sub-areas per-thread, so that there are no race conditions or performance hit, but on the expense of less flexibility in thread area size. The maxThreads parameter determines the maximum number of concurrent threads that can send life-sign reports, and it directly affects the size of each thread-area. Each thread-area is managed as a ring buffer, such that when all area is used up, new records begin to overwrite old records.

### Writing Context and Life-Sign Records

The API for writing context and life-sign records is straightforward:

    DbgUtilErr writeContextRecord(const char* recPtr, uint32_t recLen);
    DbgUtilErr writeLifeSignRecord(const char* recPtr, uint32_t recLen);

Both calls are thread-safe, with the context record API incurring probable slight performance hit due to concurrency. On the other-hand, the life-sign record API does not touch any shared resources, but only a thread-local ring-buffer, and is expected to incur minimal performance hit. This is in-line with the design concept, that context records are rather rare, and pertain to infrequent global events (e.g. thread start/end), while life-sign reports may be quite frequent and invoked by many threads concurrently.

Both context records and life-sign records keep record boundary when writing records via writeContextRecord() and writeLifeSignRecord(), such that when reading records (see below), the original record length is known.

### Closing The Life-Sign Manager

When the application is about to exit without any crash, it should close the shared segment if the life-sign manager:

    DbgUtilErr closeLifeSignShmSegment(bool deleteShm = false);

It is possible at this occasion also to delete the shared memory segment. On Windows, that means deleting the backing file, and on POSIX-compliant systems, this means unlinking the shared memory segment.

### Listing Existing Life-Sign Segments

An external application may wish to list all active and inactive shared memory segments of current and past processes. This can be done as follows:

    dbgutil::ShmSegmentList shmObjects;
    dbgutil::DbgUtilErr rc = dbgutil::getLifeSignManager()->listLifeSignShmSegments(shmObjects);

Each item in this list can be used for further inspection.

### Inspecting Life-Sign Segments

An external application may wish to inspect life-sign shared memory segments of running and terminated processes. This can be done by opening the shared memory segment through the life-sign manager:

    DbgUtilErr openLifeSignShmSegment(const char* segmentName, uint32_t totalSize, bool allowWrite,
                                      bool allowMapBackingFile = false);

This will open an existing shared memory segment. The total size needs to be known by the caller. This information is available when [listing existing segments](#listing-existing-life-sign-segments). Each element in the returned list is a pair of segment name and total size.

From this point onward, the application can inspect the shared memory segment. The first step would be to read the main header:

    DbgUtilErr readLifeSignHeader(LifeSignHeader*& hdr);

The header contains several important details regarding the shared memory segment. Most notably, the maximum number of threads is required for further inspecting life-sign records. Context records may be read one-by-one using the following API:

    DbgUtilErr readContextRecord(uint32_t& offset, char*& recPtr, uint32_t& recLen);

The caller should start with offset initialized to zero, and continue reading records until DBGUTIL_END_OF_STREAM is returned:

    uint32_t offset = 0;
    char* recPtr = nullptr;
    uint32_t recLen = 0;
    bool done = false;
    while (!done) {
        dbgutil::DbgUtilErr rc =
            dbgutil::getLifeSignManager()->readContextRecord(offset, recPtr, recLen);
        if (rc == DBGUTIL_ERR_END_OF_STREAM) {
            done = true;
        } else if (rc != DBGUTIL_ERR_OK) {
            printf("Failed to read context record at offset %u: %s\n", offset, dbgutil::errorToString(rc));
            return ERR_READ_SHM;
        } else {
            // user can process context record pointed by recPtr with length recLen
        }
    }

Reading life-sign records requires more work, specifically getting the details for each thread slot. For this, the maximum number of threads needs to be retrieved from the main segment header:

    dbgutil::DbgUtilErr rc = dbgutil::getLifeSignManager()->readLifeSignHeader(LifeSignHeader*& hdr);
    if (rc != DBGUTIL_ERR_OK) {
        printf("Failed to read life-sign header: %s\n", dbgutil::errorToString(rc));
    }

Now the life-sign records of each thread can be examined as follows:

    uint64_t threadId = 0;
    int64_t startEpoch = 0;
    int64_t endEpoch = 0;
    bool isRunning = false;
    uint32_t useCount = 0;

    for (uint32_t threadSlotId = 0; threadSlotId < hdr->m_maxThreads; ++threadSlotId) {
        dbgutil::DbgUtilErr rc = dbgutil::getLifeSignManager()->readThreadLifeSignDetails(
            threadSlotId, threadId, startEpoch, endEpoch, isRunning, useCount);
        if (rc != DBGUTIL_ERR_OK) {
            printf("Failed to read life-sign details of thread at slot %u: %s", threadSlotId, dbgutil::errorToString(rc));
            return ERR_READ_SHM;
        }

        if (useCount == 0) {
            // slot never been used
            continue;
        }

        // user can print thread details (id, start/end time, etc.)
        // user can print life-sign records (see below)
    }

The last part is reading life-sign records, which is very similar to reading context record. All ring-buffer details are hidden from the user:

    uint32_t offset = 0;
    char* recPtr = nullptr;
    uint32_t recLen = 0;
    bool done = false;
    bool callerShouldRelease = false;
    while (!done) {
        dbgutil::DbgUtilErr rc = dbgutil::getLifeSignManager()->readLifeSignRecord(
            threadSlotId, offset, recPtr, recLen, callerShouldRelease);
        if (rc == DBGUTIL_ERR_END_OF_STREAM) {
            break;
        }
        if (rc != DBGUTIL_ERR_OK) {
            printf("Failed to read context record at offset %u: %s\n", offset, dbgutil::errorToString(rc));
            return ERR_READ_SHM;
        }

        // user can process life-sign record here, if text records was saved, then it can be printed directly

        // release life-sign record if needed
        if (callerShouldRelease) {
            dbgutil::getLifeSignManager()->releaseLifeSignRecord(recPtr);
        }
    }

Note that with life-sign records the caller may occasionally need to release the returned record, since life-sign records reside in a cyclic buffer and may cross buffer boundaries and wrap around.

### Keeping Windows Life-Sign Segments In-Sync

Unlike POSIX-compliant systems, on Windows, a shared memory segment is deleted when the process crashes, unless another process has an open handle to the segment. For this reason, it would be good if the backing file would be in-sync with the shared memory contents. For this purpose the following API was added:

    dbgutil::DbgUtilErr rc = dbgutil::getLifeSignManager()->syncLifeSignShmSegment();

This call incurs a performance penalty, as it requires flushing shared memory contents to disk, so it is recommended to make this call from a background thread in the application, or from an external process (see below for more details).

### Keeping Windows Life-Sign Segments Alive

Periodically synchronizing shared memory segment contents to disk on Windows platforms is a good step forward, but not enough. A much better approach would be to keep the shared memory segment alive, by opening a handle from another process. For this purpose, some guardian process is required to make sure that shared memory segments do not get deleted. Such a guardian process may also regularly synchronize each monitored shared memory segment to disk. 

The [ELog Logging Framework](https://github.com/oa-333/elog) contains such a utility out of the box, along with a CLI for managing life-sign shared memory segments. The ELog Shared Memory Guardian does the following:

- Periodically checks for new shared memory segments
- Opens handles to each segment, ensuring the kernel object is not deleted if the owner process crashes
- Regularly synchronizes segments to disk, such that if the guardian crashes the backing file may contain up to date data
- Detects when processes terminate and writes contents of their segment to disk, such that even if the guardian crashes, the backing file on disk is already fully synchronized to disk and is ready ready for later inspection

The [ELog Logging Framework](https://github.com/oa-333/elog) Life-Sign CLI is a cross-platform tool that allows performing the following operations:

- Listing all available shared memory segments
- Dumping the contents of a shared memory segment
- Deleting a shared memory segment (with a possible backing file)
- Deleting all shared memory segments

For more information, refer to the [ELog Documentation](https://github.com/oa-333/elog#documentation).