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
#include "Bee/Core/Meta.hpp"

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

constexpr size_t kibibytes(const size_t amount) noexcept
{
    return 1024 * amount;
}

constexpr size_t mebibytes(const size_t amount) noexcept
{
    return (1024 * 1024) * amount;
}

constexpr size_t gibibytes(const size_t amount) noexcept
{
    return (1024 * 1024 * 1024) * amount;
}

inline bool is_aligned(const void* ptr, const size_t alignment) noexcept
{
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

BEE_FORCE_INLINE size_t round_up(const size_t value, const size_t pow2_byte_boundary)
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

BEE_CORE_API size_t get_page_size() noexcept;

BEE_CORE_API size_t get_min_stack_size() noexcept;

BEE_CORE_API size_t get_max_stack_size() noexcept;

BEE_CORE_API size_t get_canonical_stack_size() noexcept;

BEE_CORE_API bool guard_memory(void* memory, size_t num_bytes, MemoryProtectionMode protection) noexcept;


} // namespace bee
