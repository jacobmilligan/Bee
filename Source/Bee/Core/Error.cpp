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


namespace bee {
namespace detail {


struct ScopedRecursionGuard
{
    static constexpr i32 per_thread_recursion_limit = 2; // one for thread + internal error if needed

    static thread_local i32 count;

    ScopedRecursionGuard()
    {
        ++count;
    }

    ~ScopedRecursionGuard()
    {
        --count;
    }
};

thread_local i32 ScopedRecursionGuard::count = 0;

#define BEE_RECURSION_GUARD ScopedRecursionGuard assert_guard; if (assert_guard.count >= ScopedRecursionGuard::per_thread_recursion_limit) return


void log_assert_message(
    const char* assert_msg,
    const char* file,
    const int line,
    const char* expr,
    const char* user_fmt,
    va_list user_args
)
{
    String msg_string;
    io::StringStream msg_stream(&msg_string);

    msg_stream.write_fmt("%s", assert_msg);

    if (expr != nullptr)
    {
        msg_stream.write_fmt(" (%s)", expr);
    }

    if (user_fmt != nullptr)
    {
        msg_stream.write(" with `");
        msg_stream.write_v(user_fmt, user_args);
        msg_stream.write("`");
    }

    msg_stream.write_fmt(" at %s:%d", file, line);
    msg_stream.write("\nstack trace:\n");

    StackTrace trace{};
    capture_stack_trace(&trace, 16, 2);
    write_stack_trace(trace, &msg_stream, 2);

    log_error("%s", msg_stream.c_str());
}


void __bee_assert_handler(const char* file, const int line, const char* expr)
{
    BEE_RECURSION_GUARD;
    log_assert_message("Assertion failed", file, line, expr, nullptr, nullptr);
}


void __bee_assert_handler(
    const char* file,
    const int line,
    const char* expr,
    const char* msgformat,
    ...
)
{
    BEE_RECURSION_GUARD;

    va_list args;
    va_start(args, msgformat);
    log_assert_message("Assertion failed", file, line, expr, msgformat, args);
    va_end(args);
}

void __bee_unreachable_handler(const char* file, int line, const char* msgformat, ...)
{
    BEE_RECURSION_GUARD;

    // Unreachable code always exits even in a release build
    va_list args;
    va_start(args, msgformat);
    log_assert_message("Unreachable code executed",  file, line, nullptr, msgformat, args);
    va_end(args);
}

void __bee_check_handler(const char* file, const int line, const char* expr, const char* msgformat, ...)
{
    BEE_RECURSION_GUARD;

    va_list args;
    va_start(args, msgformat);
    log_assert_message("Check failed", file, line, expr, msgformat, args);
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
