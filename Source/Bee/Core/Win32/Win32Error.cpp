//
//  Win32Error.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 2/08/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Win32/MinWindows.h"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Debug.hpp"

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

    log_error(
        "Skyrocket: Unhandled exception: %s [at: %p]",
        exception_code_to_string(ex_info->ExceptionRecord->ExceptionCode),
        ex_info->ExceptionRecord->ExceptionAddress
    );

    log_stack_trace(LogVerbosity::error, 1); // skip `top_level_exception_filter`

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



} // namespace bee
