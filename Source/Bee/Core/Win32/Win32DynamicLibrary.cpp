/*
 *  Win32DynamicLibrary.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/Error.hpp"
#include "Bee/Core/Win32/MinWindows.h"

namespace bee {


DynamicLibrary load_library(const char* path)
{
    const auto handle = LoadLibraryA(path);
    BEE_ASSERT_F(handle != nullptr, "unable to load dynamic library at %s: %s", path, win32_get_last_error_string());
    return { handle };
}

bool unload_library(const DynamicLibrary& library)
{
    const auto free_success = FreeLibrary(static_cast<HMODULE>(library.handle));
    BEE_ASSERT_F(free_success != 0, "unable to unload dynamic library: %s", win32_get_last_error_string());
    return free_success != 0;
}

void* get_library_symbol(const DynamicLibrary& library, const char* symbol_name)
{
    const auto address = GetProcAddress(static_cast<HMODULE>(library.handle), symbol_name);
    BEE_ASSERT_F(address != nullptr, "unable to get symbol address: %s: %s", symbol_name, win32_get_last_error_string());
    return reinterpret_cast<void*>(address);
}


} // namespace bee