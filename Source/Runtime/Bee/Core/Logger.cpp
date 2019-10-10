/*
 *  Log.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Debug.hpp"
#include "Bee/Core/IO.hpp"

#if BEE_OS_WINDOWS == 1
    #define BEE_MINWINDOWS_ENABLE_SHELLAPI
    #include "Bee/Core/Win32/MinWindows.h"
#endif // BEE_OS_WINDOWS == 1


namespace bee {


void default_logger_callback(const LogVerbosity verbosity, const char* fmt, va_list va_args);

struct LogSystem
{
    RecursiveSpinLock                   system_mutex;
    SpinLock                            stdio_mutex;
    LogVerbosity                        verbosity { LogVerbosity::debug };
    DynamicArray<logger_callback_t*>    registered_loggers;

#if BEE_OS_WINDOWS == 1

    bool        is_gui_console { false };
    bool        console_is_setup { false };
    HANDLE      stdout_handle { nullptr };
    HANDLE      stderr_handle { nullptr };
    HANDLE      stdin_handle { nullptr };

#endif // BEE_OS_WINDOWS == 1

    ~LogSystem()
    {
#if BEE_OS_WINDOWS == 1

        if (console_is_setup)
        {
            FreeConsole();
        }

#endif // BEE_OS_WINDOWS == 1
    }
};


static LogSystem g_log;


/*
 * Log - implementations
 */

#if BEE_OS_WINDOWS == 1

void ensure_console()
{
    if (g_log.console_is_setup)
    {
        return;
    }

    /*
     * Check if there's a console already allocated for the current process by trying to attach to it. If
     * `AttachConsole` fails with ERROR_INVALID_HANDLE it means there's no console allocated so we have to do it
     * ourselves with `AllocConsole`, otherwise if it fails with any other error it means there was an error attaching
     * to the already existing console.
     * See: https://docs.microsoft.com/en-us/windows/console/attachconsole
     */
    auto success = AttachConsole(ATTACH_PARENT_PROCESS);
    if (success == FALSE)
    {
        switch (GetLastError())
        {
            case ERROR_ACCESS_DENIED:
            {
                success = TRUE; // already attached to a console
                break;
            }
            case ERROR_INVALID_HANDLE:
            {
                success = AllocConsole(); // no console allocated
                break;
            }
            default:
            {
                break;
            }
        }
    }

    if (success == TRUE)
    {
        g_log.stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        g_log.stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
        g_log.stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
        SetConsoleMode(g_log.stdin_handle, ENABLE_WINDOW_INPUT);
        g_log.console_is_setup = true;
        return;
    }

    OutputDebugString("Unable to attach to console: ");

    // Output the error string
    char error_buffer[1024]{};
    static constexpr auto error_buffer_size = static_array_length(error_buffer);
    const auto error_code = GetLastError();
    const auto formatting_options = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

    FormatMessage(
        formatting_options,                         // allocate a buffer large enough for the message and lookup system messages
        nullptr,                                    // source of the message - null as we're looking up system messages
        error_code,                                 // message ID for the requested message
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // default language
        error_buffer,                               // destination for the message buffer
        static_cast<DWORD>(error_buffer_size),      // buffer size
        nullptr                                     // no arguments
    );

    OutputDebugString(error_buffer);
}


// Outputs to the currently attached win32 console - `write_buffer` must be null-terminated
void win32_write_console(const LogVerbosity verbosity, const char* write_buffer, const i32 write_buffer_length)
{
    ensure_console();

    if (verbosity == LogVerbosity::quiet)
    {
        return;
    }

    auto handle = verbosity == LogVerbosity::error ? g_log.stderr_handle : g_log.stdout_handle;

    WriteConsole(handle, write_buffer, write_buffer_length, nullptr, nullptr);

    if (is_debugger_attached())
    {
        OutputDebugString(write_buffer);
    }
}

#endif

void default_logger_callback(const LogVerbosity verbosity, const char* fmt, va_list va_args)
{
    scoped_spinlock_t lock(g_log.stdio_mutex);

#if BEE_OS_WINDOWS == 1
    static constexpr int write_buffer_size = 4096; // 4K

    char write_buffer[write_buffer_size];
    io::StringStream stream(write_buffer, write_buffer_size, 0);
    stream.write_v(fmt, va_args);
    stream.write("\n");
    win32_write_console(verbosity, stream.c_str(), stream.size());
#else
    vfprintf(file, fmt, va_args);
#endif // BEE_OS_WINDOWS == 1
}

void ensure_logger()
{
    if (g_log.registered_loggers.empty())
    {
        logger_init();
    }
}

void logger_init()
{
    static bool recursion_check = false;
    if (!recursion_check)
    {
        recursion_check = true;
        log_register_callback(default_logger_callback);
        recursion_check = false;
    }
}

void log_set_verbosity(const LogVerbosity verbosity)
{
    ensure_logger();

    scoped_recursive_spinlock_t lock(g_log.system_mutex);
    g_log.verbosity = verbosity;
}

LogVerbosity log_get_verbosity()
{
    ensure_logger();

    scoped_recursive_spinlock_t lock(g_log.system_mutex);
    return g_log.verbosity;
}

void log_register_callback(logger_callback_t* logger)
{
    ensure_logger();

    scoped_recursive_spinlock_t lock(g_log.system_mutex);

    // Ensure loggers aren't registered twice
    for (const auto& registered_logger : g_log.registered_loggers)
    {
        if (BEE_FAIL_F(registered_logger != logger, "Tried to register the same logger callback more than once"))
        {
            return;
        }
    }

    g_log.registered_loggers.push_back(logger);
}

void write_v(const LogVerbosity verbosity, const char* fmt, va_list va_args)
{
    ensure_logger();

    scoped_recursive_spinlock_t lock(g_log.system_mutex);

    for (auto& log_callback : g_log.registered_loggers)
    {
        log_callback(verbosity, fmt, va_args);
    }
}

void log_info(const char* fmt, ...)
{
    if (g_log.verbosity < LogVerbosity::info)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    write_v(LogVerbosity::info, fmt, args);
    va_end(args);
}

void log_warning(const char* fmt, ...)
{
    if (g_log.verbosity < LogVerbosity::warn)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    write_v(LogVerbosity::warn, fmt, args);
    va_end(args);
}

void log_error(const char* fmt, ...)
{
    if (g_log.verbosity < LogVerbosity::error)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    write_v(LogVerbosity::error, fmt, args);
    va_end(args);
}

void log_debug(const char* fmt, ...)
{
    if (g_log.verbosity < LogVerbosity::debug)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    write_v(LogVerbosity::debug, fmt, args);
    va_end(args);
}


void log_write(LogVerbosity verbosity, const char* fmt, ...)
{
    if (g_log.verbosity < verbosity)
    {
        return;
    }

    va_list args;
    va_start(args, fmt);
    write_v(verbosity, fmt, args);
    va_end(args);
}


void log_write_v(LogVerbosity verbosity, const char* fmt, va_list va_args)
{
    if (g_log.verbosity < verbosity)
    {
        return;
    }

    write_v(verbosity, fmt, va_args);
}


} // namespace bee
