# retain original compile flags in case we need to clear to default for
# specific targets (i.e. third party header libs)
set(bee_default_c_flags ${CMAKE_C_FLAGS})
set(bee_default_cxx_flags ${CMAKE_CXX_FLAGS})
set(bee_compile_flags "")

# turn on warnings
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()

    if(CMAKE_C_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    endif()

    # Global flags used by all builds
    set(msvc_default_flags
        /DWIN32         # CMake compatibility
        /D_WINDOWS      # CMake compatibility
        /GR-            # disable RTTI
        /EHsc-          # disable exceptions
    )

    string(REPLACE ";" " " cxx_flags "${msvc_default_flags}")
    set(CMAKE_CXX_FLAGS "${cxx_flags}" CACHE STRING "C++ default flags" FORCE)

    # Global flags used by debug builds
    set(msvc_debug_flags
        /MDd            # link the multithreaded and DLL specific msvcrt
        /Zi             # generate PDB's
        /Zo             # generate enhanced debugging info - needed due to disabling inline expansion
        /Od             # turn off optimizations
        /Ob0            # disable inlining expansion
        /RTC1           # enable stack frame run-time error checks and unintialized variable usage
        /GS             # enable buffer security checks
        /DDEBUG
        /D_DEBUG
    )

    # Global flags used by release builds
    set(msvc_release_flags
        /MD             # link the multithreaded and DLL specific msvcrt
        /Zi             # generate PDB's
        /Zo             # generate enhanced debugging info - needed due to disabling inline expansion
        /Ox             # maximum speed without /Gy & /Gf
        /GS-            # turn off buffer security checks
        /DNDEBUG
        /D_RELEASE
    )

    string(REPLACE ";" " " cxx_flags_debug "${msvc_debug_flags}")
    string(REPLACE ";" " " cxx_flags_release "${msvc_release_flags}")

    set(CMAKE_C_FLAGS_DEBUG "${cxx_flags_debug}" CACHE STRING "C flags" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "${cxx_flags_debug}" CACHE STRING "C++ flags" FORCE)

    set(CMAKE_C_FLAGS_RELEASE "${cxx_flags_release}" CACHE STRING "C flags" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "${cxx_flags_release}" CACHE STRING "C++ flags" FORCE)

    # Flags added to Skyrocket targets only
    set(bee_compile_flags
            /W4             # set error level to 4
            /WX             # warnings are errors
            /MP             # multi-processor compilation
            /wd4201         # enable anonymous struct
            /wd4100         # disable 'identifier' : unreferenced formal parameter
            /wd4267         # disable 'var' : conversion from 'size_t' to 'type', possible loss of data
        )

    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    list(APPEND bee_compile_flags
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -fno-rtti

        -Wall
        -Werror
        -Wno-unused-function
    )

    if (WIN32)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    endif ()
endif()

# check for ASAN & TSAN availability
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if (USE_ASAN)
        bee_global_log("Compiling with LLVM Address Sanitizer")
        list(APPEND bee_compile_flags "-fsanitize=address" "-fno-omit-frame-pointer")
    endif ()

    if (USE_TSAN)
        bee_global_log("Compiling with LLVM Thread Sanitizer")
        list(APPEND bee_compile_flags "-fsanitize=thread" "-O1" "-fno-omit-frame-pointer")
    endif ()
endif()

# Add compile definitions for the build type
add_compile_definitions(
        $<$<CONFIG:Debug>:BEE_DEBUG=1>
        $<$<CONFIG:Release>:BEE_RELEASE=1>
)

function(__bee_set_compile_options target)
    get_target_property(old_options ${target} COMPILE_OPTIONS)
    if(old_options MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "" new_options "${old_options}")
    endif()
    set_target_properties(${target} PROPERTIES COMPILE_OPTIONS "${new_options}")
    target_compile_options(${target} PRIVATE ${bee_compile_flags})
endfunction()
