//
//  MinWindows.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 2/06/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Win32/MinWindows.h"

namespace bee {


const char* win32_format_error(const int error_code,  char* dst_buffer, const int buffer_size)
{
    const auto formatting_options = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

    FormatMessage(
        formatting_options,                         // allocate a buffer large enough for the message and lookup system messages
        nullptr,                                    // source of the message - null as we're looking up system messages
        error_code,                                 // message ID for the requested message
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // default language
        dst_buffer,                                 // destination for the message buffer
        static_cast<DWORD>(buffer_size),            // buffer size
        nullptr                                     // no arguments
    );

    return dst_buffer;
}


const char* win32_get_last_error_string(char* dst_buffer, const int buffer_size)
{
    return win32_format_error(GetLastError(), dst_buffer, buffer_size);
}

const char* win32_get_last_error_string()
{
    static constexpr int message_buffer_size = 1024;
    static char message_buffer[message_buffer_size]{0};
    return win32_get_last_error_string(message_buffer, message_buffer_size);
}


} // namespace bee