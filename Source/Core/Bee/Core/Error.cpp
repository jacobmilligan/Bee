/*
 *  Error.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Debug.hpp"
#include "Bee/Core/IO.hpp"

#include <stdlib.h>


namespace bee {
namespace detail {


static bool assert_guard = false;


#define BEE_ASSERT_GUARD if (assert_guard) { return; } assert_guard = true


void log_assert_message(
    const char* assert_msg,
    const char* function,
    const char* file,
    const int line,
    const char* expr,
    const char* user_fmt,
    va_list user_args
)
{
    String msg_string(temp_allocator());
    io::StringStream msg_stream(&msg_string);

    msg_stream.write_fmt("Bee: %s", assert_msg);

    if (expr != nullptr)
    {
        msg_stream.write_fmt(" (%s)", expr);
    }

    msg_stream.write_fmt("\n\tat %s:%d\n\tin function %s", file, line, function);

    if (user_fmt != nullptr)
    {
        msg_stream.write_fmt("\n\treason: ");
        msg_stream.write_v(user_fmt, user_args);
    }

    msg_stream.write("\n");

    StackTrace trace{};
    capture_stack_trace(&trace, 16, 2);
    write_stack_trace(trace, &msg_stream);

    log_error("%s", msg_stream.c_str());
}


void __bee_assert_handler(const char* function, const char* file, const int line, const char* expr)
{
    BEE_ASSERT_GUARD;
    log_error("Bee: Assertion failed (%s)", expr);
    log_error("at %s:%d in function %s\n", file, line, function);
    log_stack_trace(LogVerbosity::error, 1);
}


void __bee_assert_handler(
    const char* function,
    const char* file,
    const int line,
    const char* expr,
    const char* msgformat,
    ...
)
{
    BEE_ASSERT_GUARD;

    va_list args;
    va_start(args, msgformat);
    log_assert_message("Assertion failed", function, file, line, expr, msgformat, args);
    va_end(args);
}

void __bee_print_error(
    bool with_trace,
    const char* func,
    const char* file,
    const int line,
    const char* type,
    const char* msgformat,
    ...
)
{
    String msg_string(temp_allocator());
    io::StringStream stream(&msg_string);

    stream.write_fmt("Bee %s error: ", type);

    va_list args;
    va_start(args, msgformat);

    stream.write_v(msgformat, args);

    va_end(args);

    stream.write_fmt(" at %s:%d\n\tin function %s", file, line, func);
    log_error("%s", stream.c_str());
    if (with_trace)
    {
        log_stack_trace(LogVerbosity::error, 1);
    }
}

void __bee_unreachable_handler(const char* function, const char* file, int line, const char* msgformat, ...)
{
    BEE_ASSERT_GUARD;

    // Unreachable code always exits even in a release build
    va_list args;
    va_start(args, msgformat);
    log_assert_message("Unreachable code executed", function, file, line, nullptr, msgformat, args);
    va_end(args);
}

void __bee_check_handler(
    const char* function,
    const char* file,
    const int line,
    const char* expr,
    const char* msgformat,
    ...
)
{
    BEE_ASSERT_GUARD;

    va_list args;
    va_start(args, msgformat);
    log_assert_message("Check failed", function, file, line, expr, msgformat, args);
    va_end(args);
}

[[noreturn]] void __bee_abort()
{
    abort();
}

void __bee_abort_handler()
{
BEE_PUSH_WARNING
BEE_DISABLE_WARNING_MSVC(4702)

    // This way we don't have to #include <stdlib.h> in Error.hpp
    abort();

BEE_POP_WARNING
}



} // namespace detail
} // namespace bee
