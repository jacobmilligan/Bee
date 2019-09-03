//
//  Win32StackTrace.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 3/08/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

/*
 * Most of the functionality here is adapted from these MSDN pages:
 * https://docs.microsoft.com/en-us/windows/win32/debug/using-dbghelp
 *
 * There's not a lot of information about using the DbgHelp library.
 */

#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/Debug.hpp"
#include "Bee/Core/Win32/MinWindows.h"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/String.hpp"

#include <DbgHelp.h>

namespace bee {

// DbgHelp function types
using RtlCaptureStackBackTrace_function_t = WORD(
    DWORD   FramesToSkip,
    DWORD   FramesToCapture,
    PVOID*  BackTrace,
    PDWORD  BackTraceHash
);

using SymInitialize_function_t = BOOL(
    HANDLE  hProcess,
    PCSTR   UserSearchPath,
    BOOL    fInvadeProcess
);

using SymCleanup_function_t = BOOL(HANDLE hProcess);

using SymSetOptions_function_t = DWORD(DWORD SymOptions);

using SymFromAddr_function_t = BOOL(
    HANDLE          hProcess,
    DWORD64         qwAddr,
    PDWORD64        pdwDisplacement,
    PSYMBOL_INFO    Symbol
);

using SymGetLineFromAddr64_function_t = BOOL(
    HANDLE          hProcess,
    DWORD64         dwAddr,
    PDWORD          pdwDisplacement,
    PIMAGEHLP_LINE  Line
);

using SymSetSearchPath_function_t = BOOL(
    HANDLE hProcess,
    PCSTR  NewSearchPath
);

using SymGetSearchPath_function_t = BOOL(
    HANDLE hProcess,
    PSTR   DstSearchPath,
    DWORD  SearchPathLength
);

using SymRefreshModuleList_function_t = BOOL(HANDLE hProcess);

using SymGetModuleInfo64_function_t = BOOL(
    HANDLE             hProcess,
    DWORD64            qwAddr,
    PIMAGEHLP_MODULE64 ModuleInfo
);

using UnDecorateSymbolName_function_t = DWORD(
    PCSTR name,
    PSTR  outputString,
    DWORD maxStringLength,
    DWORD flags
);

// Holds library module handle for ntdll and the related Dbghelp functions
struct Win32DbgHelp
{
    static constexpr DWORD SymOptions = SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES;

    // Libraries
    DynamicLibrary                          ntdll;
    DynamicLibrary                          dbhelp;

    // Procs
    RtlCaptureStackBackTrace_function_t*    fp_RtlCaptureStackBackTrace { nullptr };
    SymInitialize_function_t*               fp_SymInitialize { nullptr };
    SymCleanup_function_t*                  fp_SymCleanup { nullptr };
    SymSetOptions_function_t*               fp_SymSetOptions { nullptr };
    SymFromAddr_function_t*                 fp_SymFromAddr { nullptr };
    SymGetLineFromAddr64_function_t*        fp_SymGetLineFromAddr64 { nullptr };
    SymGetSearchPath_function_t*            fp_SymGetSearchPath { nullptr };
    SymSetSearchPath_function_t*            fp_SymSetSearchPath { nullptr };
    SymRefreshModuleList_function_t*        fp_SymRefreshModuleList { nullptr };
    SymGetModuleInfo64_function_t*          fp_SymGetModuleInfo64 { nullptr };
    UnDecorateSymbolName_function_t*        fp_UnDecorateSymbolName { nullptr };


    // Win32DbgHelp data
    bool                                    initialized { false };
    RecursiveSpinLock                       mutex;

    Win32DbgHelp()
    {
        if (initialized)
        {
            return;
        }

        scoped_recursive_spinlock_t  lock(mutex);

        initialized = true;

        ntdll = load_library("ntdll.dll"); // for RtlCaptureStackBackTrace
        dbhelp = load_library("dbghelp.dll"); // for all other functions

        // Get symbols
        fp_RtlCaptureStackBackTrace = (RtlCaptureStackBackTrace_function_t*)get_library_symbol(ntdll, "RtlCaptureStackBackTrace");

        fp_SymInitialize = (SymInitialize_function_t*)get_library_symbol(dbhelp, "SymInitialize");
        fp_SymCleanup = (SymCleanup_function_t*)get_library_symbol(dbhelp, "SymCleanup");
        fp_SymSetOptions = (SymSetOptions_function_t*)get_library_symbol(dbhelp, "SymSetOptions");
        fp_SymFromAddr = (SymFromAddr_function_t*)get_library_symbol(dbhelp, "SymFromAddr");
        fp_SymGetLineFromAddr64 = (SymGetLineFromAddr64_function_t*)get_library_symbol(dbhelp, "SymGetLineFromAddr64");
        fp_SymRefreshModuleList = (SymRefreshModuleList_function_t*)get_library_symbol(dbhelp, "SymRefreshModuleList");
        fp_SymGetModuleInfo64 = (SymGetModuleInfo64_function_t*)get_library_symbol(dbhelp, "SymGetModuleInfo64");
        fp_UnDecorateSymbolName = (UnDecorateSymbolName_function_t*)get_library_symbol(dbhelp, "UnDecorateSymbolName");

        /*
         * Initialize the symbol handler
         * see: https://docs.microsoft.com/en-us/windows/win32/debug/initializing-the-symbol-handler
         */
        fp_SymSetOptions(SymOptions);

        const auto syminitialize_success = fp_SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        if (BEE_FAIL_F(syminitialize_success == TRUE, "Win32Debug: Failed to initialize the symbol handler: %s", win32_get_last_error_string()))
        {
            return;
        }
    }

    ~Win32DbgHelp()
    {
        scoped_recursive_spinlock_t  lock(mutex);

        if (!initialized)
        {
            return;
        }

        if (fp_SymCleanup(GetCurrentProcess()) != TRUE)
        {
            BEE_ERROR("Win32Debug", "Failed to cleanup symbol resources: %s", win32_get_last_error_string());
            BEE_DEBUG_BREAK();
        }

        unload_library(ntdll);
        unload_library(dbhelp);
        initialized = false;
    }
};


// Statically initialized to avoid having to explicitly call `init_stacktracing` or similar
static Win32DbgHelp g_dbghelp;


bool is_debugger_attached()
{
    return IsDebuggerPresent() != 0;
}

void refresh_debug_symbols()
{
    if (!g_dbghelp.initialized)
    {
        return;
    }

    scoped_recursive_spinlock_t lock(g_dbghelp.mutex);
    const auto refresh_symbols_success = g_dbghelp.fp_SymRefreshModuleList(GetCurrentProcess());
    BEE_ASSERT_F(refresh_symbols_success, "Win32Debug: Failed to refresh symbols: %s", win32_get_last_error_string());
}

void capture_stack_trace(StackTrace* trace, i32 captured_frame_count, i32 skipped_frame_count)
{
    if (!g_dbghelp.initialized)
    {
        return;
    }

    BEE_ASSERT(trace != nullptr);
    BEE_ASSERT(captured_frame_count <= StackTrace::max_frame_count);

    scoped_recursive_spinlock_t  lock(g_dbghelp.mutex);

    ULONG backtrace_hash = 0;
    trace->frame_count = g_dbghelp.fp_RtlCaptureStackBackTrace(
        static_cast<DWORD>(1 + skipped_frame_count),
        static_cast<DWORD>(captured_frame_count),
        trace->frames,
        &backtrace_hash
    );

    BEE_ASSERT(trace->frame_count <= StackTrace::max_frame_count);
}

/*
 * Adapted from https://docs.microsoft.com/en-us/windows/win32/debug/retrieving-symbol-information-by-address.
 */
void symbolize_stack_trace(DebugSymbol* dst_symbols, const StackTrace& trace, const i32 frame_count)
{
    if (!g_dbghelp.initialized)
    {
        return;
    }

    static constexpr i32 syminfo_name_length = 1024;
    static constexpr i32 syminfo_buffer_size = sizeof(SYMBOL_INFO) + syminfo_name_length * sizeof(TCHAR);

    char syminfo_buffer[syminfo_buffer_size];
    auto syminfo = reinterpret_cast<SYMBOL_INFO*>(syminfo_buffer);

    ZeroMemory(syminfo, sizeof(IMAGEHLP_SYMBOL64));

    syminfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    syminfo->MaxNameLen = syminfo_name_length;

    auto process_handle = GetCurrentProcess();

    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    IMAGEHLP_MODULE64 module{};
    module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

    DWORD64 sym_displacement = 0;
    DWORD line_displacement = 0;

    scoped_recursive_spinlock_t  lock(g_dbghelp.mutex);

    const auto frames_to_symbolize = frame_count > 0 ? frame_count : trace.frame_count;

    for (int f = 0; f < frames_to_symbolize; ++f)
    {
        auto addr = (DWORD64)trace.frames[f];
        auto success = g_dbghelp.fp_SymFromAddr(process_handle, addr, &sym_displacement, syminfo);

        if (success == FALSE)
        {
            BEE_ERROR("StackTrace", "Failed to retrieve symbol info at address %p: %s", trace.frames[f], win32_get_last_error_string());
            BEE_DEBUG_BREAK();
            continue;
        }

        auto& symbol = dst_symbols[f];
        symbol.module_name[0] = '\0';
        symbol.filename[0] = '\0';
        symbol.function_name[0] = '\0';
        symbol.address = trace.frames[f];
        symbol.line = -1;

        success = g_dbghelp.fp_SymGetLineFromAddr64(process_handle, addr, &line_displacement, &line);
        if (success != FALSE)
        {
            symbol.line = line.LineNumber;
            str::copy(symbol.filename, DebugSymbol::name_size, line.FileName);
        }
        else
        {
            str::copy(symbol.filename, DebugSymbol::name_size, syminfo->Name, syminfo->NameLen);
        }

        success = g_dbghelp.fp_SymGetModuleInfo64(process_handle, addr, &module);
        if (success == TRUE)
        {
            str::copy(symbol.module_name, DebugSymbol::name_size, module.ModuleName);
        }

        const auto undecorate_result = g_dbghelp.fp_UnDecorateSymbolName(
            syminfo->Name,
            symbol.function_name,
            DebugSymbol::name_size,
            UNDNAME_COMPLETE
        );

        if (undecorate_result == 0)
        {
            BEE_ERROR("StackTrace", "Failed to get function name for address %p: %s", trace.frames[f], win32_get_last_error_string());
        }
    }
}


} // namespace bee
