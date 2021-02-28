/*
 *  Win32String.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/String.hpp"

#define BEE_MINWINDOWS_ENABLE_NLS
#include "Bee/Core/Win32/MinWindows.h"

#include <string.h>

namespace bee {
namespace str {

/*
 ****************************************************************
 *
 * Win32 - String conversion implementations
 *
 *****************************************************************
 */
String from_wchar(const wchar_t* wchar_str, Allocator* allocator)
{
    const auto byte_count = WideCharToMultiByte(
        CP_UTF8,    // utf8 codepage
        0,          // no flags
        wchar_str,  // src string
        -1,         // process whole string to get length - assumes null-termination
        nullptr,    // no buffer - just getting length
        0,          // no buffer size
        nullptr,    // default char if unable to match character - use system default
        nullptr     // we don't need to know if a default char has been used
    );

    // process whole string to get length - assumes null-termination
    return from_wchar(wchar_str, byte_count, allocator);
}

String from_wchar(const wchar_t* wchar_str, const i32 byte_size, Allocator* allocator)
{
    String result(allocator);
    from_wchar(&result, wchar_str, byte_size);
    return BEE_MOVE(result);
}

void from_wchar(String* dst, const wchar_t* wchar_str, const i32 byte_size)
{
    int length = 0;
    for (int i = 0; i < byte_size; ++i)
    {
        if (wchar_str[i] == '\0')
        {
            break;
        }
        ++length;
    }

    const auto offset = dst->size();
    dst->insert(offset, byte_size, '\0');

    const auto utf8_size = WideCharToMultiByte(
        CP_UTF8,                        // utf-8 codepage
        0,                              // no flags
        wchar_str,                      // utf16 string
        length,                         // utf16 string length
        dst->data() + offset,           // utf8 string
        byte_size,                      // utf8 string capacity
        nullptr,                        // default char
        nullptr                         // was the default char used?
    );

    dst->resize(utf8_size);
}

i32 from_wchar(char* dst, const i32 dst_size, const wchar_t* wchar_str, const i32 wchar_size)
{
    return WideCharToMultiByte(
        CP_UTF8,    // utf8 codepage
        0,          // no flags
        wchar_str,  // src string
        wchar_size, // string length
        dst,        // no buffer - just getting length
        dst_size,   // no buffer size
        nullptr,    // default char if unable to match character - use system default
        nullptr     // we don't need to know if a default char has been used
    );
}


wchar_array_t to_wchar(const StringView& src, Allocator* allocator)
{
    if (src.empty())
    {
        return wchar_array_t(allocator);
    }

    // Get num characters that will be written
    auto wstring_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src.c_str(), src.size(), nullptr, 0);
    if (BEE_FAIL_F(wstring_size != 0, "Failed to convert utf8 string to wchar string: %s", win32_get_last_error_string()))
    {
        return wchar_array_t(allocator);
    }

    // Convert to wchar - add 1 extra for null-termination
    auto result = wchar_array_t::with_size(wstring_size + 1, '\0', allocator);
    result.resize(wstring_size); // resize back to string size minus null-terminator

    wstring_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src.c_str(), src.size(), result.data(), result.size());

    if (BEE_FAIL_F(wstring_size != 0, "Failed to convert UTF-8 string to wchar string: %s", win32_get_last_error_string()))
    {
        return wchar_array_t(allocator);
    }

    return result;
}

i32 to_wchar(const StringView& src, wchar_t* buffer, const i32 buffer_size)
{
    if (src.empty())
    {
        return 0;
    }

    // Get num characters that will be written
    auto wstring_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src.c_str(), src.size(), nullptr, 0);
    if (BEE_FAIL_F(wstring_size != 0, "Failed to convert utf8 string to wchar string: %s", win32_get_last_error_string()))
    {
        return 0;
    }

    if (buffer == nullptr || buffer_size <= 0)
    {
        return wstring_size;
    }

    // Convert to wchar - add 1 extra for null-termination
    wstring_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src.c_str(), src.size(), buffer, buffer_size - 1);
    if (BEE_FAIL_F(wstring_size != 0, "Failed to convert UTF-8 string to wchar string: %s", win32_get_last_error_string()))
    {
        return 0;
    }

    buffer[wstring_size] = '\0';
    return wstring_size;
}


} // namespace str
} // namespace bee
