/*
 *  Win32Error.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Win32/MinWindows.h"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Debug.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/IO.hpp"

namespace bee {


/*
 *************************
 *
 * Exception stages
 *
 *************************
 */
const char* exception_code_to_string(const DWORD ex_code)
{
#define BEE_EX_CODE(code, string_value) case code: return string_value
    switch (ex_code)
    {
        BEE_EX_CODE(EXCEPTION_ACCESS_VIOLATION, "access violation");
        BEE_EX_CODE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED, "array bounds exceeded");
        BEE_EX_CODE(EXCEPTION_BREAKPOINT, "breakpoint triggered");
        BEE_EX_CODE(EXCEPTION_DATATYPE_MISALIGNMENT, "datatype misalignment");
        BEE_EX_CODE(EXCEPTION_FLT_DENORMAL_OPERAND, "floating point operand is denormal");
        BEE_EX_CODE(EXCEPTION_FLT_DIVIDE_BY_ZERO, "floating point divide by zero");
        BEE_EX_CODE(EXCEPTION_FLT_INEXACT_RESULT, "inexact floating point result");
        BEE_EX_CODE(EXCEPTION_FLT_INVALID_OPERATION, "invalid floating point operation");
        BEE_EX_CODE(EXCEPTION_FLT_OVERFLOW, "floating point overflow");
        BEE_EX_CODE(EXCEPTION_FLT_STACK_CHECK, "floating point stack check");
        BEE_EX_CODE(EXCEPTION_FLT_UNDERFLOW, "floating point underflow");
        BEE_EX_CODE(EXCEPTION_ILLEGAL_INSTRUCTION, "illegal instruction");
        BEE_EX_CODE(EXCEPTION_IN_PAGE_ERROR, "invalid page error");
        BEE_EX_CODE(EXCEPTION_INT_DIVIDE_BY_ZERO, "integer divide by zero");
        BEE_EX_CODE(EXCEPTION_INT_OVERFLOW, "integer overflow");
        BEE_EX_CODE(EXCEPTION_INVALID_DISPOSITION, "invalid disposition");
        BEE_EX_CODE(EXCEPTION_NONCONTINUABLE_EXCEPTION, "noncontinuable exception");
        BEE_EX_CODE(EXCEPTION_PRIV_INSTRUCTION, "priv instruction");
        BEE_EX_CODE(EXCEPTION_SINGLE_STEP, "single step");
        BEE_EX_CODE(EXCEPTION_STACK_OVERFLOW, "stack overflow");
        default: break;
    }
#undef BEE_EX_CODE

    return "unknown exception";
}

BEE_PUSH_WARNING
BEE_DISABLE_WARNING_MSVC(4702)
LONG top_level_exception_filter(PEXCEPTION_POINTERS ex_info)
{
    static bool recursive_exception_check = false;
    if (recursive_exception_check)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    recursive_exception_check = true;

    String msg;
    io::StringStream stream(&msg);

    stream.write_fmt(
        "Unhandled exception `%s` [%p]\nstack trace:\n",
        exception_code_to_string(ex_info->ExceptionRecord->ExceptionCode),
        ex_info->ExceptionRecord->ExceptionAddress
    );

    StackTrace trace{};
    capture_stack_trace(&trace, 12, 1); // skip `top_level_exception_filter`
    write_stack_trace(trace, &stream, 2);

    log_error("%s", msg.c_str());

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1

    detail::__bee_abort_handler();

#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    recursive_exception_check = false;
    return EXCEPTION_EXECUTE_HANDLER;
}
BEE_POP_WARNING

void enable_exception_handling()
{
    SetUnhandledExceptionFilter(&top_level_exception_filter);
}

void disable_exception_handling()
{
    SetUnhandledExceptionFilter(nullptr);
}


/*
 *************************
 *
 * Signal handling
 *
 *************************
 */
BOOL WINAPI win32_ctrl_handler(DWORD ctrl_type)
{
    switch (ctrl_type)
    {
        case CTRL_CLOSE_EVENT:
        {
            log_warning("Close console requested");
            return TRUE;
        }

        case CTRL_C_EVENT:
        {
            detail::__bee_abort_handler();
            return TRUE;
        }

        case CTRL_BREAK_EVENT:
        {
            return FALSE;
        }

        case CTRL_LOGOFF_EVENT:
        {
            return FALSE;
        }

        case CTRL_SHUTDOWN_EVENT:
        {
            return FALSE;
        }

        default: break;
    }

    return FALSE;
}


void init_signal_handler()
{
    if (!SetConsoleCtrlHandler(win32_ctrl_handler, TRUE))
    {
        log_error("Failed to initialize signal handler: %s", win32_get_last_error_string());
    }
}



} // namespace bee
