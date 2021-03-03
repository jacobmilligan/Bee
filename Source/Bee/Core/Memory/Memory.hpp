/*
 *  Memory.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */
#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/Enum.hpp"

#if BEE_OS_WINDOWS == 1
    BEE_PUSH_WARNING
        BEE_DISABLE_PADDING_WARNINGS
        #include <malloc.h>
    BEE_POP_WARNING
#endif // BEE_OS_WINDOWS == 1


namespace bee {


BEE_FLAGS(MemoryProtectionMode, u8) {
    none    = 0u,
    read    = 1u << 0u,
    write   = 1u << 1u,
    exec    = 1u << 2u
};

/*
* Helpers
*/

constexpr size_t kilobytes(const size_t amount) noexcept
{
    return 1024 * amount;
}

constexpr size_t megabytes(const size_t amount) noexcept
{
    return (1024 * 1024) * amount;
}

constexpr size_t gigabytes(const size_t amount) noexcept
{
    return (1024 * 1024 * 1024) * amount;
}

inline bool is_aligned(const void* ptr, const size_t alignment) noexcept
{
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

constexpr size_t round_up(const size_t value, const size_t pow2_byte_boundary)
{
    return (value + pow2_byte_boundary - 1) & ~(pow2_byte_boundary - 1);
}

// Undefined if alignment is not a power of 2
BEE_FORCE_INLINE void* align(void* ptr, const size_t alignment)
{
// warning C4146: unary minus operator applied to unsigned type, result still unsigned.
// thanks MSVC - **I know** - this is intentional here.
    const auto intptr = reinterpret_cast<uintptr_t>(ptr);
BEE_PUSH_WARNING
BEE_DISABLE_WARNING_MSVC(4146)
    const auto aligned = (intptr - 1u + alignment) & -alignment;
BEE_POP_WARNING

    return reinterpret_cast<void*>(aligned);
}


// Must be a macro because even a FORCE_INLINE function may possibly free the memory
#if BEE_OS_WINDOWS == 1
    #define BEE_ALLOCA(Size, Alignment) _alloca(bee::round_up(Size, Alignment))
#else
    #define BEE_ALLOCA(Size, Alignment) alloca(bee::round_up(Size, Alignment))
#endif // BEE_OS_WINDOWS == 1

#define BEE_ALLOCA_ARRAY(T, Size) static_cast<T*>(BEE_ALLOCA(sizeof(T) * Size, alignof(T)))


BEE_CORE_API size_t get_page_size() noexcept;

BEE_CORE_API size_t get_min_stack_size() noexcept;

BEE_CORE_API size_t get_max_stack_size() noexcept;

BEE_CORE_API size_t get_canonical_stack_size() noexcept;

BEE_CORE_API bool guard_memory(void* memory, size_t num_bytes, MemoryProtectionMode protection) noexcept;


} // namespace bee
