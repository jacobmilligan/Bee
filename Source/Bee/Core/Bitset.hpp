/*
 *  Bitset.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/NumericTypes.hpp"

#include <string.h>

namespace bee {

template <u32 Size>
struct Bitset {
    using array_t = u32[Size];

    static constexpr u32    data_size = 32 - 1;
    array_t                 bits{};

    BEE_FORCE_INLINE Bitset()
    {
        clear_all();
    }

    BEE_FORCE_INLINE constexpr u32 byte_position(const u32 bit) const noexcept
    {
        return bit >> 5; // log2(32)
    }

    BEE_FORCE_INLINE bool is_set(const u32 bit) const noexcept
    {
        return (bits[byte_position(bit)] & (1 << (bit & data_size))) != 0;
    }

    BEE_FORCE_INLINE void set_bit(const u32 bit) noexcept
    {
        bits[byte_position(bit)] |= 1 << (bit & data_size);
    }

    BEE_FORCE_INLINE void clear_bit(const u32 bit) noexcept
    {
        bits[byte_position(bit)] &= ~(1 << (bit & data_size));
    }

    BEE_FORCE_INLINE void toggle_bit(const u32 bit) noexcept
    {
        bits[byte_position(bit)] ^= 1 << (bit & data_size);
    }

    BEE_FORCE_INLINE void set_all() noexcept
    {
        memset(bits, UINT32_MAX, Size * sizeof(ui32));
    }

    BEE_FORCE_INLINE void clear_all() noexcept
    {
        memset(bits, 0, Size * sizeof(uint32_t));
    }
};


} // namespace bee
