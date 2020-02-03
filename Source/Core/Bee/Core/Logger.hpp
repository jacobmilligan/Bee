/*
 *  Log.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

#include <stdarg.h>
#include <stdio.h>

namespace bee {


enum class LogVerbosity : u8
{
    quiet   = 0,
    info    = 1,
    warn    = 2,
    error   = 3,
    debug   = 4
};

using logger_callback_t = void(const LogVerbosity verbosity, const char* fmt, va_list va_args);

BEE_CORE_API void logger_init();

BEE_CORE_API void logger_shutdown();

BEE_CORE_API void log_set_verbosity(LogVerbosity verbosity);

BEE_CORE_API LogVerbosity log_get_verbosity();

BEE_CORE_API void log_register_callback(logger_callback_t* logger);

BEE_CORE_API void log_info(const char* fmt, ...) BEE_PRINTFLIKE(1, 2);

BEE_CORE_API void log_warning(const char* fmt, ...) BEE_PRINTFLIKE(1, 2);

BEE_CORE_API void log_error(const char* fmt, ...) BEE_PRINTFLIKE(1, 2);

BEE_CORE_API void log_debug(const char* fmt, ...) BEE_PRINTFLIKE(1, 2);

BEE_CORE_API void log_write(LogVerbosity verbosity, const char* fmt, ...) BEE_PRINTFLIKE(2, 3);

BEE_CORE_API void log_write_v(LogVerbosity verbosity, const char* fmt, va_list va_args);


} // namespace bee