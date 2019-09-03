/*
 *  DynamicLibrary.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"

namespace bee {


struct DynamicLibrary
{
#if BEE_OS_WINDOWS == 1
    static constexpr auto file_extension = ".dll";
#elif BEE_OS_MACOS == 1
    static constexpr auto file_extension = ".dylib";
#else
    #error Platform unsupported
#endif // BEE_OS_*

    void* handle { nullptr };
};

BEE_CORE_API DynamicLibrary load_library(const char* path);

BEE_CORE_API bool unload_library(const DynamicLibrary& library);

BEE_CORE_API void* get_library_symbol(const DynamicLibrary& library, const char* symbol_name);


} // namespace bee