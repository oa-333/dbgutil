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
            dbg_util_flags.h
            dbg_util_log.h
            dbg_util.h
            life_sign_manager.h
            os_exception_handler.h
            os_module_manager.h
            os_shm.h
            os_stack_trace.h
            os_symbol_engine.h
            os_thread_manager.h)