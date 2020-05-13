/*
 *  Error.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"


#if BEE_COMPILER_GCC == 1
    #include <signal.h>
#endif // BEE_COMPILER_GCC == 1

#ifndef BEE_CONFIG_ENABLE_ASSERTIONS
    #if BEE_DEBUG
        #define BEE_CONFIG_ENABLE_ASSERTIONS 1
    #else
        #define BEE_CONFIG_ENABLE_ASSERTIONS 0
    #endif // BEE_DEBUG
#endif // BEE_CONFIG_ENABLE_ASSERTIONS

namespace bee {
namespace detail {


/// @brief Handles the assertion macro. Prints the format assertion string to stderr
/// @param function The function the assertion occurred in
/// @param file The file the assertion occurred in
/// @param line The line the assertion occurred in
/// @param expr String representing the assertion that failed
/// @param msgformat The message format string
/// @param ... Format arguments
BEE_CORE_API void bee_assert_handler(
    const char* file,
    int line,
    const char* expr,
    const char* msgformat,
    ...
) BEE_PRINTFLIKE(4, 5);

BEE_CORE_API void bee_assert_handler(const char* file, int line, const char* expr);

BEE_CORE_API void bee_unreachable_handler(
    const char* file,
    int line,
    const char* msgformat,
    ...
) BEE_PRINTFLIKE(3, 4);

BEE_CORE_API void bee_check_handler(
    const char* file,
    int line,
    const char* expr,
    const char* msgformat,
    ...
) BEE_PRINTFLIKE(4, 5);


[[noreturn]] BEE_CORE_API void bee_abort();

BEE_CORE_API void bee_abort_handler();


}  // namespace detail

/// Stops the debugger at the given point if it's attached
#if BEE_DEBUG
    #if BEE_COMPILER_CLANG == 1
        BEE_FORCE_INLINE void clang_debugbreak() { asm("int $3"); }
        #define BEE_DEBUG_BREAK() bee::clang_debugbreak()
    #elif BEE_COMPILER_GCC == 1
        #define BEE_DEBUG_BREAK() raise(SIGTRAP)
    #elif BEE_COMPILER_MSVC == 1
        #define BEE_DEBUG_BREAK() __debugbreak()
    #else
        #error No debug break instruction implemented for the current platform
    #endif // BEE_COMPILER_*
#else
    #define BEE_DEBUG_BREAK() ((void)0)
#endif

/*
 * Defines a line of code as one that should never be reached - this will always abort and g_log an error
 * even with BEE_CONFIG_ENABLE_ASSERTIONS turned off
 */
#define BEE_UNREACHABLE(msgformat, ...)                             \
        BEE_BEGIN_MACRO_BLOCK                                       \
            bee::detail::bee_unreachable_handler(                   \
                __FILE__, __LINE__, msgformat, ##__VA_ARGS__        \
            );                                                      \
            if (bee::is_debugger_attached()) BEE_DEBUG_BREAK();     \
            bee::detail::bee_abort();                               \
        BEE_END_MACRO_BLOCK


/**
 * Assert macros - If BEE_CONFIG_ENABLE_ASSERTIONS == 1 (generally debug builds) these macros
 * will check the given expression and crash the application if it evaluates to false.
 * If BEE_CONFIG_ENABLE_ASSERTIONS == 0 (release, dev builds etc.) the macros expand to a no-op
 */
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1

    #define BEE_ASSERT(expr)                                                            \
        BEE_BEGIN_MACRO_BLOCK                                                           \
            if(BEE_UNLIKELY(!(expr))) {                                                 \
                bee::detail::bee_assert_handler(__FILE__, __LINE__, #expr);             \
                if (bee::is_debugger_attached()) BEE_DEBUG_BREAK();                     \
                bee::detail::bee_abort();                                               \
            }                                                                           \
        BEE_END_MACRO_BLOCK

    #define BEE_ASSERT_NO_DEBUG_BREAK(expr)                                 \
        BEE_BEGIN_MACRO_BLOCK                                               \
            if(BEE_UNLIKELY(!(expr))) {                                     \
                bee::detail::bee_assert_handler(__FILE__, __LINE__, #expr); \
                bee::detail::bee_abort();                                   \
            }                                                               \
        BEE_END_MACRO_BLOCK

    #define BEE_ASSERT_F(expr, msgformat, ...)                                      \
        BEE_BEGIN_MACRO_BLOCK                                                       \
            if(BEE_UNLIKELY(!(expr))) {                                             \
                bee::detail::bee_assert_handler(__FILE__, __LINE__,                 \
                                            #expr, msgformat, ##__VA_ARGS__);       \
                if (bee::is_debugger_attached()) BEE_DEBUG_BREAK();                 \
                bee::detail::bee_abort();                                           \
            }                                                                       \
        BEE_END_MACRO_BLOCK

    #define BEE_ASSERT_F_NO_DEBUG_BREAK(expr, msgformat, ...)                               \
        BEE_BEGIN_MACRO_BLOCK                                                               \
            if(BEE_UNLIKELY(!(expr))) {                                                     \
                bee::detail::bee_assert_handler(__FILE__, __LINE__,                       \
                                            #expr, msgformat, ##__VA_ARGS__);               \
                bee::detail::bee_abort();                                                 \
            }                                                                               \
        BEE_END_MACRO_BLOCK

#else

#define BEE_ASSERT(expr) ((void)sizeof(expr))
#define BEE_ASSERT_NO_DEBUG_BREAK(expr) ((void)sizeof(expr))
#define BEE_ASSERT_F(expr, msgformat, ...) ((void)sizeof(expr))
#define BEE_ASSERT_F_NO_DEBUG_BREAK(expr, msgformat, ...) ((void)sizeof(expr))

#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

/**
 * Check/Fail macros - these check the given expression, if false, will return false and log an
 * error message, otherwise it will evaluate to true and do nothing else. This allows them to be
 * used inside conditional code like so:
 * ```
 * if (BEE_CHECK_F(some_value == 23, "This is the wrong value!")) {
 *     do_thing_with_value(some_value);
 * }
 * ```
 * The Fail_* variants check to see if the expression given fails to pass as true and will return
 * true if so, which allows using checks for early outs on fail conditions, for example:
 * ```
 * if (BEE_FAIL_F(ptr != nullptr, "Pointer was null when it shouldn't have been!")) {
 *     return ErrorCode::invalid_ptr;
 * }
 * return ErrorCode::success;
 * ```
 *
 * Additionally, if BEE_CONFIG_ENABLE_ASSERTIONS == 1 these macros will behave like BEE_ASSERT
 * and crash the program if a CHECK doesn't pass or a FAIL is triggered.
 */

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1


#define BEE_CHECK(expr) (BEE_LIKELY((expr))                                                                 \
    || (bee::detail::bee_check_handler(__FILE__, __LINE__, #expr, nullptr, nullptr),   \
        !bee::is_debugger_attached() || (BEE_DEBUG_BREAK(), true), bee::detail::bee_abort_handler(), false))

#define BEE_CHECK_F(expr, msg, ...) (BEE_LIKELY((expr))                                                     \
    || (bee::detail::bee_check_handler(__FILE__, __LINE__, #expr, msg, ##__VA_ARGS__), \
        !bee::is_debugger_attached() || (BEE_DEBUG_BREAK(), true), bee::detail::bee_abort_handler(), false))

#define BEE_FAIL(expr) !(BEE_UNLIKELY((expr))                                                               \
    || (bee::detail::bee_check_handler(__FILE__, __LINE__, #expr, nullptr, nullptr),   \
        !bee::is_debugger_attached() || (BEE_DEBUG_BREAK(), true), bee::detail::bee_abort_handler(), false))

#define BEE_FAIL_F(expr, msg, ...) !(BEE_UNLIKELY((expr))                                                   \
    || (bee::detail::bee_check_handler(__FILE__, __LINE__, #expr, msg, ##__VA_ARGS__), \
        !bee::is_debugger_attached() || (BEE_DEBUG_BREAK(), true), bee::detail::bee_abort_handler(), false))

#else

#define BEE_CHECK(expr) (BEE_LIKELY((expr))                                                                 \
    || (bee::detail::bee_check_handler(__FILE__, __LINE__, #expr, nullptr, nullptr),   \
        false))

#define BEE_CHECK_F(expr, msg, ...) (BEE_LIKELY((expr))                                                     \
    || (bee::detail::bee_check_handler(__FILE__, __LINE__, #expr, msg, ##__VA_ARGS__), \
        false))

#define BEE_FAIL(expr) !(BEE_UNLIKELY((expr))                                                               \
    || (bee::detail::bee_check_handler(__FILE__, __LINE__, #expr, nullptr, nullptr),   \
        false))

#define BEE_FAIL_F(expr, msg, ...) !(BEE_UNLIKELY((expr))                                                   \
    || (bee::detail::bee_check_handler(__FILE__, __LINE__, #expr, msg, ##__VA_ARGS__), \
        false))

#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1


/// @brief Formats an error and help message for printing in a static_assert
#define BEE_STATIC_ASSERT_MSG(error_msg, help_msg) error_msg "\n===Help===> " help_msg "\n"


/**
 * Enables the Bee exception handler instead of the default system one if the platform supports it to allow
 * asserting on exceptions rather than throwing
 */
BEE_CORE_API void enable_exception_handling();

/**
 * Disables the Bee exception handler if it's enabled
 */
BEE_CORE_API void disable_exception_handling();

/**
 * Initializes the console signal handler for graceful terminations in console apps
 */
BEE_CORE_API void init_signal_handler();

/**
 * Returns true if a debugger is attached to the executable being run
 */
BEE_CORE_API bool is_debugger_attached();


} // namespace bee