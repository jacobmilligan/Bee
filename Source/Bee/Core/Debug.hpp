/*
 *  StackTrace.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

/*
 ************************************************************************************
 *
 * !!! IMPORTANT !!
 *
 * DO NOT include any headers here that also include `Allocator.hpp`. In builds with
 * BEE_CONFIG_ENABLE_MEMORY_TRACKING set to 1, `Allocator.hpp` includes `MemoryTracker.hpp`
 * which then includes this header in order to support retrieving stack traces in the
 * public interface. If you #include `Allocator.hpp` in this file you WILL get a
 * circular dependency and all the indecipherable errors that come with it.
 * This isn't an ideal situation but it's fine for now.
 *
 *************************************************************************************
 */

#include "Bee/Core/Logger.hpp"


namespace bee {
namespace io {


class StringStream;


}

struct StackTrace
{
    static constexpr i32 max_frame_count = 64;

    i32 frame_count { 0 };
    void* frames[max_frame_count];
};

struct DebugSymbol
{
    static constexpr i32 name_size = 256;

    void*   address { nullptr };
    i32     line { -1 };
    char    module_name[name_size];
    char    filename[name_size];
    char    function_name[name_size];
};

BEE_API bool is_debugger_attached();

BEE_API void refresh_debug_symbols();

/**
 * Captures a trace of all the stack frame addresses from the calling site back. To turn these addresses into
 * human-readable symbols use `symbolize_stack_trace`
 */
BEE_API void capture_stack_trace(StackTrace* trace, i32 captured_frame_count, i32 skipped_frame_count);

/**
 * Takes a previously captured `StackTrace` and converts their addresses into human-readable `DebugSymbol` objects.
 * **Note:** the `dst_symbols` buffer must have a size of at least `trace.frame_count`
 */
BEE_API void symbolize_stack_trace(DebugSymbol* dst_symbols, const StackTrace& trace, i32 frame_count = 0);

/**
 * Logs a stack trace to the loggers matching `verbosity` level output (i.e. if verbosity == info, it will
 * output to log::info), skipping `skipped_frame_count` frames before starting
 */
BEE_API void log_stack_trace(LogVerbosity verbosity, i32 skipped_frame_count = 0);

BEE_API void write_stack_trace(const StackTrace& trace, io::StringStream* stream);


} // namespace bee
