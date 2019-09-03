/*
 *  Config.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include <stddef.h>

namespace bee {


/// @defgroup platform Platform module
#ifndef BEE_DEBUG
    #define BEE_DEBUG 0
#endif // BEE_DEBUG

#ifndef BEE_RELEASE
    #define BEE_RELEASE 0
#endif // BEE_RELEASE

/// @brief MacOSX operating system
#define BEE_OS_MACOS 0

/// @brief IOS operating system
#define BEE_OS_IOS 0

/// @brief Android operating system
#define BEE_OS_ANDROID 0

/// @brief Windows operating system
#define BEE_OS_WINDOWS 0

/// @brief Linux operating systems
#define BEE_OS_LINUX 0

/// @}

#if defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>

    #if TARGET_IPHONE_SIMULATOR == 1
        #undef BEE_OS_IOS
        #define BEE_OS_IOS 1
        #define BEE_OS_NAME_STRING "iOS Simulator"
    #elif TARGET_OS_IPHONE == 1
        #undef BEE_OS_IOS
        #define BEE_OS_IOS 1
        #define BEE_OS_NAME_STRING "iOS"
    #elif TARGET_OS_MAC == 1
        #undef BEE_OS_MACOS
        #define BEE_OS_MACOS 1
        #define BEE_OS_NAME_STRING "MacOS"
    #endif // TARGET_*
#elif defined(__WIN32__) || defined(__WINDOWS__) || defined(_WIN64) \
 || defined(_WIN32) || defined(_WINDOWS) || defined(__TOS_WIN__)
    #undef BEE_OS_WINDOWS
    #define BEE_OS_WINDOWS 1
    #define BEE_OS_NAME_STRING "Windows"
#elif defined(__linux__) || defined(__linux) || defined(linux_generic)
    #undef BEE_OS_LINUX
    #define BEE_OS_LINUX 1
    #define BEE_OS_NAME_STRING "Linux"
#elif defined(__ANDROID__)
    #undef BEE_OS_ANDROID
    #define BEE_OS_ANDROID 1
    #define BEE_ANDROID_API_LEVEL __ANDROID_API__
    #define BEE_OS_NAME_STRING "Android"
#endif // defined(<platform>)

#if BEE_OS_LINUX == 1 || BEE_OS_MACOS == 1 || BEE_OS_IOS == 1 || BEE_OS_ANDROID == 1
    #define BEE_OS_UNIX 1
#else
    #define BEE_OS_UNIX 0
#endif //

#ifndef BEE_OS_NAME_STRING
    #define BEE_OS_NAME_STRING "UNKNOWN_OS"
#endif // ifndef BEE_OS_NAME_STRING

/*
* Compiler detection
*/

#define BEE_COMPILER_CLANG 0
#define BEE_COMPILER_GCC 0
#define BEE_COMPILER_MSVC 0
#define BEE_COMPILER_UNKNOWN 1

#if defined(__clang__)
    #undef BEE_COMPILER_UNKNOWN
    #define BEE_COMPILER_UNKNOWN 0

    #undef BEE_COMPILER_CLANG
    #define BEE_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #undef BEE_COMPILER_UNKNOWN
    #define BEE_COMPILER_UNKNOWN 0

    #undef BEE_COMPILER_GCC
    #define BEE_COMPILER_GCC 1
#elif defined(_MSC_VER)
    #undef BEE_COMPILER_UNKNOWN
    #define BEE_COMPILER_UNKNOWN 0

    #undef BEE_COMPILER_MSVC
    #define BEE_COMPILER_MSVC 1
#endif // defined(_MSC_VER)

/*
* Compiler implementations
*/

#define BEE_STRINGIFY(x) #x


/*
 * Clang and GCC shared definitions
 */
#if BEE_COMPILER_CLANG == 1 || BEE_COMPILER_GCC == 1

    #define BEE_PACKED(n)                       __attribute__((packed, aligned(n)))

    #define BEE_FUNCTION_NAME                   __PRETTY_FUNCTION__

    #define BEE_FORCE_INLINE inline             __attribute__((always_inline))

    #define BEE_PRINTFLIKE(fmt, firstvararg)    __attribute__((__format__ (__printf__, fmt, firstvararg)))

    #define BEE_EXPORT_SYMBOL                   __attribute__ ((visibility("default")))

    #define BEE_DISABLE_WARNING_MSVC(w)

    #define BEE_UNUSED                          __attribute__((unused))

    #define BEE_LIKELY(statement)               __builtin_expect((statement), 1)

    #define BEE_UNLIKELY(statement)             __builtin_expect((statement), 0)

#endif // BEE_COMPILER_CLANG || BEE_COMPILER_GCC

#if BEE_COMPILER_CLANG == 1

    #define BEE_PUSH_WARNING                _Pragma("clang diagnostic push")

    #define BEE_POP_WARNING                 _Pragma("clang diagnostic pop")

    #define BEE_DISABLE_WARNING_CLANG(w)    _Pragma(BEE_STRINGIFY(clang diagnostic ignored w))

#elif BEE_COMPILER_GCC == 1

    #define BEE_DISABLE_WARNING_CLANG(w)

#elif BEE_COMPILER_MSVC == 1
    // Displays type information, calling signature etc. much like __PRETTY_FUNCTION__
    // see: https://msdn.microsoft.com/en-us/library/b0084kay.aspx
    #define BEE_FUNCTION_NAME                   __FUNCSIG__

    #define BEE_FORCE_INLINE                    __forceinline

    #define BEE_PRINTFLIKE(fmt, firstvararg)

    #ifdef BEE_DLL
        #define BEE_EXPORT_SYMBOL                   __declspec(dllexport)
        #define BEE_IMPORT_SYMBOL                   __declspec(dllimport)
    #endif // BEE_DLL

    #define BEE_PUSH_WARNING                    __pragma(warning( push ))
    #define BEE_DISABLE_WARNING_MSVC(w)         __pragma(warning( disable: w ))
    #define BEE_POP_WARNING                     __pragma(warning( pop ))

    #define BEE_DISABLE_WARNING_CLANG(w)

    #define BEE_UNUSED

    #define BEE_LIKELY(statement)               (statement)

    #define BEE_UNLIKELY(statement)             (statement)

    #if _WIN64
    #else
        #define BEE_ARCH_32BIT
    #endif // _WIN64
#endif // BEE_COMPILER_*

/*
 * Processor architecture - this only works for x86_64 currently.
 * see: http://nadeausoftware.com/articles/2012/02/c_c_tip_how_detect_processor_type_using_compiler_predefined_macros
 */
#if defined(__x86_64__) || defined(_M_X64)
    #define BEE_ARCH_64BIT 1
    #define BEE_ARCH_BITS 64
#else
    #define BEE_ARCH_32BIT 1
    #define BEE_ARCH_BITS 32
#endif // Processor arch


#ifndef BEE_EXPORT_SYMBOL
    #define BEE_EXPORT_SYMBOL
#endif // BEE_EXPORT_SYMBOL

#ifndef BEE_IMPORT_SYMBOL
    #define BEE_IMPORT_SYMBOL
#endif // BEE_IMPORT_SYMBOL

/*
 * DLL API macro for import/export of the Bee library
 */
#ifdef BEE_BUILD_EXPORT_LIBRARY
    #define BEE_API BEE_EXPORT_SYMBOL
#else
    #define BEE_API BEE_IMPORT_SYMBOL
#endif

/*
* Graphics API definitions
*/

#if !defined(BEE_CONFIG_METAL_SUPPORT)
    #define BEE_CONFIG_METAL_SUPPORT 0
#endif // !defined(BEE_CONFIG_METAL_SUPPORT)

#if !defined(BEE_CONFIG_OPENGL_SUPPORT)
    #define BEE_CONFIG_OPENGL_SUPPORT 0
#endif // !defined(BEE_CONFIG_OPENGL_SUPPORT)

#if !defined(BEE_CONFIG_D3D9_SUPPORT)
    #define BEE_CONFIG_D3D9_SUPPORT 0
#endif // !defined(BEE_CONFIG_D3D9_SUPPORT)

#if !defined(BEE_CONFIG_D3D11_SUPPORT)
    #define BEE_CONFIG_D3D11_SUPPORT 0
#endif // !defined(BEE_CONFIG_D3D11_SUPPORT)

#if !defined(BEE_CONFIG_D3D12_SUPPORT)
    #define BEE_CONFIG_D3D12_SUPPORT 0
#endif // !defined(BEE_CONFIG_D3D12_SUPPORT)

#if !defined(BEE_CONFIG_VULKAN_SUPPORT)
    #define BEE_CONFIG_VULKAN_SUPPORT 0
#endif // !defined(BEE_CONFIG_VULKAN_SUPPORT)

/*
* Build configuration options
*/
#if !defined(BEE_CONFIG_DEFAULT_TEMP_ALLOCATOR_SIZE)
    #define BEE_CONFIG_DEFAULT_TEMP_ALLOCATOR_SIZE 4194304 // 4MB
#endif // BEE_CONFIG_DEFAULT_TEMP_ALLOCATOR_SIZE

#if !defined(BEE_CONFIG_MOCK_TEST_DATA)
    #define BEE_CONFIG_MOCK_TEST_DATA 0
#endif // BEE_CONFIG_MOCK_TEST_DATA


#if BEE_CONFIG_FORCE_MEMORY_TRACKING == 1
    #define BEE_CONFIG_ENABLE_MEMORY_TRACKING BEE_CONFIG_FORCE_MEMORY_TRACKING
#else
    #if BEE_DEBUG == 1 && BEE_CONFIG_ENABLE_MEMORY_TRACKING == 1
        #define BEE_CONFIG_ENABLE_MEMORY_TRACKING 1
    #else
        #ifndef BEE_CONFIG_ENABLE_MEMORY_TRACKING
            #define BEE_CONFIG_ENABLE_MEMORY_TRACKING 0
        #endif // BEE_CONFIG_ENABLE_MEMORY_TRACKING
    #endif // BEE_DEBUG == 1
#endif // !defined(BEE_CONFIG_FORCE_MEMORY_TRACKING)


#if BEE_CONFIG_FORCE_ASSERTIONS_ENABLED == 1
    #define BEE_CONFIG_ENABLE_ASSERTIONS BEE_CONFIG_FORCE_ASSERTIONS_ENABLED
#else
    #if BEE_DEBUG == 1
        #define BEE_CONFIG_ENABLE_ASSERTIONS 1
    #else
        #define BEE_CONFIG_ENABLE_ASSERTIONS 0
    #endif // BEE_DEBUG == 1
#endif // !defined(BEE_CONFIG_FORCE_ASSERTIONS_ENABLED)

/*
* Platform-specific helper macros
*/

#define BEE_OBJC_RELEASE

#if defined(__OBJC__)

    #undef BEE_OBJC_RELEASE
    #define BEE_OBJC_RELEASE(object) [object release], (object) = nil

#endif // defined(__OBJC__)

/*
* General helper macros
*/

#define BEE_BEGIN_MACRO_BLOCK do {

#define BEE_END_MACRO_BLOCK } while (false)

#define BEE_EXPLICIT_SCOPE(x) do { x } while (false)

/*
 * Graphics API platform macros
 */
#ifndef BEE_CONFIG_GRAPHICS_API_VULKAN
    #define BEE_CONFIG_GRAPHICS_API_VULKAN 0
#endif // BEE_CONFIG_GRAPHICS_API_VULKAN

#ifndef BEE_CONFIG_GRAPHICS_API_METAL
    #define BEE_CONFIG_GRAPHICS_API_METAL 0
#endif // BEE_CONFIG_GRAPHICS_API_METAL


template <typename T, int Size>
inline constexpr int static_array_length(T(&)[Size])
{
     return Size;
}


} // namespace bee
