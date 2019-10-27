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

    String utf8_string(byte_count - 1, '\0', allocator);
    WideCharToMultiByte(CP_UTF8, 0, wchar_str, -1, utf8_string.data(), utf8_string.size(), nullptr, nullptr);
    return utf8_string;
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


} // namespace str
} // namespace bee
