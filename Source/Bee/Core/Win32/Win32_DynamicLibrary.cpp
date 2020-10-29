/*
 *  Win32DynamicLibrary.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/DynamicLibrary.hpp"
#include "Bee/Core/Error.hpp"
#include "Bee/Core/Win32/MinWindows.h"
#include "Bee/Core/Logger.hpp"

namespace bee {


DynamicLibrary load_library(const char* path)
{
    auto* handle = LoadLibraryA(path);
    if (handle == nullptr)
    {
        log_error("unable to load dynamic library at %s: %s", path, win32_get_last_error_string());
    }
    return { handle };
}

bool unload_library(const DynamicLibrary& library)
{
    const auto free_success = FreeLibrary(static_cast<HMODULE>(library.handle));
    if (free_success == TRUE)
    {
        return true;
    }
    log_error("unable to unload dynamic library: %s", win32_get_last_error_string());
    return false;
}

void* get_library_symbol(const DynamicLibrary& library, const char* symbol_name)
{
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library.handle), symbol_name));
}


} // namespace bee