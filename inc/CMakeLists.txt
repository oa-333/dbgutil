target_sources(
    dbgutil
    PUBLIC
        FILE_SET publicheaders
        TYPE HEADERS
        FILES
            dbg_stack_trace.h
            dbg_util_def.h
            dbg_util_err.h
            dbg_util_except.h
            libdbg_flags.h
            libdbg_log.h
            libdbg.h
            os_exception_handler.h
            os_module_manager.h
            os_stack_trace.h
            os_symbol_engine.h
            os_thread_manager.h)