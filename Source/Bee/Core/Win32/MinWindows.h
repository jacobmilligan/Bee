/*
 *  MinimalWindows.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Config.hpp" // for cross-platform dllexport/import

#if defined(_WINDOWS_) && !defined(BEE_MINWINDOWS)
    #error By including Windows.h before MinimalWindows.h, all of the below will be included and result in a super bloated Win32 build
#endif // defined(_WINDOWS_)

#define BEE_MINWINDOWS

/*
 * Combo defines that allow the minimum needed to use the larger Win32 API's
 */
#if defined(BEE_MINWINDOWS_ENABLE_SHELLAPI)
    #define BEE_MINWINDOWS_ENABLE_USER
    #define BEE_MINWINDOWS_ENABLE_NLS
    #define BEE_MINWINDOWS_ENABLE_MSG
    #define BEE_MINWINDOWS_ENABLE_WINDOWING
    #define BEE_MINWINDOWS_ENABLE_CONTROL
#endif // defined(BEE_MINWINDOWS_ENABLE_SHELLAPI)

/*
 * Remove a bunch of unused stuff from Windows.h (these optional #defines were taken from Windows.h)
 */
#define NOGDICAPMASKS     // - CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // - VK_*

#if !defined(BEE_MINWINDOWS_ENABLE_WINDOWING)
    #define NOWINMESSAGES     // - WM_*, EM_*, LB_*, CB_*
    #define NOWINSTYLES       // - WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
    #define NOSHOWWINDOW      // - SW_*
#endif // !defined(BEE_MINWINDOWS_ENABLE_WINDOWING)

#if !defined(BEE_MINWINDOWS_ENABLE_USER)
    #define NOUSER            // - All USER defines and routines
#endif // !defined(BEE_MINWINDOWS_ENABLE_USER)

#if !defined(BEE_MINWINDOWS_ENABLE_GDI)
    #define NOGDI             // - All GDI defines and routines
#endif // !defined(BEE_MINWINDOWS_ENABLE_GDI)

#if !defined(BEE_MINWINDOWS_ENABLE_MSG)
//    #define NOMSG             // - typedef MSG and associated routines
#endif // !defined(BEE_MINWINDOWS_ENABLE_MSG)

#if !defined(BEE_MINWINDOWS_ENABLE_NLS)
    #define NONLS             // - All NLS (Normalized String) defines and routines
#endif // !defined(BEE_MINWINDOWS_ENABLE_NLS)

#define NOSYSMETRICS      // - SM_*
#define NOMENUS           // - MF_*
#define NOICONS           // - IDI_*
#define NOKEYSTATES       // - MK_*
#define NOSYSCOMMANDS     // - SC_*
#define NORASTEROPS       // - Binary and Tertiary raster ops
#define OEMRESOURCE       // - OEM Resource values
#define NOATOM            // - Atom Manager routines
#define NOCLIPBOARD       // - Clipboard routines
#define NOCOLOR           // - Screen colors

#if !defined(BEE_MINWINDOWS_ENABLE_CONTROL)
    #define NOCTLMGR          // - Control and Dialog routines
#endif // !defined(BEE_MINWINDOWS_ENABLE_CONTROL)

#define NODRAWTEXT        // - DrawText() and DT_*
#define NOKERNEL          // - All KERNEL defines and routines
#define NOMB              // - MB_* and MessageBox()
#define NOMEMMGR          // - GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // - typedef METAFILEPICT
#define NOOPENFILE        // - OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // - SB_* and scrolling routines
#define NOSERVICE         // - All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // - Sound driver routines
#define NOTEXTMETRIC      // - typedef TEXTMETRIC and associated routines
#define NOWH              // - SetWindowsHook and WH_*
#define NOWINOFFSETS      // - GWL_*, GCL_*, associated routines
#define NOCOMM            // - COMM driver routines
#define NOKANJI           // - Kanji support stuff.
#define NOHELP            // - Help engine interface.
#define NOPROFILER        // - Profiler interface.
#define NODEFERWINDOWPOS  // - DeferWindowPos routines
#define NOMCX             // - Modem Configuration Extensions

#define NOMINMAX          // - Macros min(a,b) and max(a,b)
#define WIN32_LEAN_AND_MEAN

#if defined(BEE_MINWINDOWS_ARCH_ONLY)
    #if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_IA64_) && !defined(_AMD64_) && !defined(_ARM_) && !defined(_ARM64_) && defined(_M_IX86)
    #define _X86_
    #if !defined(_CHPE_X86_ARM64_) && defined(_M_HYBRID)
    #define _CHPE_X86_ARM64_
    #endif
    #endif

    #if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_IA64_) && !defined(_AMD64_) && !defined(_ARM_) && !defined(_ARM64_) && defined(_M_AMD64)
    #define _AMD64_
    #endif

    #if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_IA64_) && !defined(_AMD64_) && !defined(_ARM_) && !defined(_ARM64_) && defined(_M_ARM)
    #define _ARM_
    #endif

    #if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_IA64_) && !defined(_AMD64_) && !defined(_ARM_) && !defined(_ARM64_) && defined(_M_ARM64)
    #define _ARM64_
    #endif

    #if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_IA64_) && !defined(_AMD64_) && !defined(_ARM_) && !defined(_ARM64_) && defined(_M_M68K)
    #define _68K_
    #endif

    #if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_IA64_) && !defined(_AMD64_) && !defined(_ARM_) && !defined(_ARM64_) && defined(_M_MPPC)
    #define _MPPC_
    #endif

    #if !defined(_68K_) && !defined(_MPPC_) && !defined(_X86_) && !defined(_M_IX86) && !defined(_AMD64_) && !defined(_ARM_) && !defined(_ARM64_) && defined(_M_IA64)
    #if !defined(_IA64_)
    #define _IA64_
    #endif /* !_IA64_ */
    #endif

    #ifndef _MAC
    #if defined(_68K_) || defined(_MPPC_)
    #define _MAC
    #endif
    #endif
#endif // defined(BEE_MINWINDOWS_ARCH_ONLY)

/*
 * finally Include Windows.h
 */
#if !defined(BEE_MINWINDOWS_DEFINES_ONLY) && !defined(BEE_MINWINDOWS_ARCH_ONLY)
    #include <Windows.h>
#endif // !defined(BEE_MINWINDOWS_DEFINES_ONLY)

namespace bee {


BEE_API const char* win32_format_error(const int error_code,  char* dst_buffer, const int buffer_size);

BEE_API const char* win32_get_last_error_string(char* dst_buffer, int buffer_size);

BEE_API const char* win32_get_last_error_string();


} // namespace bee