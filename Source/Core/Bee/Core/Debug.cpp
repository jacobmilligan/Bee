/*
 *  StackTrace.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Debug.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/IO.hpp"

namespace bee {


String stack_trace_to_string(const StackTrace& trace, Allocator* allocator)
{
    String trace_string(allocator);
    io::StringStream stream(&trace_string);
    write_stack_trace(trace, &stream);
    return trace_string;
}


void log_stack_trace(const LogVerbosity verbosity, const i32 skipped_frame_count)
{
    StackTrace trace{};
    capture_stack_trace(&trace, 12, 1 + skipped_frame_count);

    const auto trace_string = stack_trace_to_string(trace, temp_allocator());
    log_write(verbosity, "%s", trace_string.c_str());
}


void write_stack_trace(const StackTrace& trace, io::StringStream* stream)
{
    auto symbols = FixedArray<DebugSymbol>::with_size(trace.frame_count);
    symbolize_stack_trace(symbols.data(), trace);

    for (int f = 0; f < symbols.size(); ++f)
    {
        if (symbols[f].line >= 0)
        {
            stream->write_fmt(
                "%d: [%p] %s!%s\n\tat %s:%d",
                f,
                symbols[f].address,
                symbols[f].module_name,
                symbols[f].function_name,
                symbols[f].filename,
                symbols[f].line
            );
        }
        else
        {
            stream->write_fmt(
                "%d: [%p] %s!%s",
                f,
                symbols[f].address,
                symbols[f].module_name,
                symbols[f].function_name
            );
        }

        stream->write("\n");
    }
}


} // namespace bee