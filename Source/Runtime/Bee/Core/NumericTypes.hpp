/*
 *  IntTypes.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Error.hpp"

#include <stdint.h>

namespace bee {

/*
 * Integer type declarations
 */
using i8        = int8_t;
using i16       = int16_t;
using i32       = int32_t;
using i64       = int64_t;

using u8        = uint8_t;
using u16       = uint16_t;
using u32       = uint32_t;
using u64       = uint64_t;

using isize     = intptr_t;


/*
 * u128 - 128 bit numeric type
 */
struct u128
{
#if defined(BEE_LITTLE_ENDIAN)
    u64 low { 0 };
    u64 high { 0 };
#elif defined(BEE_BIG_ENDIAN)
    u64 high { 0 };
    u64 low { 0 };
#else
    #error Unsupported byte order
#endif // BEE_LITTLE_ENDIAN

    constexpr u128() = default;

    constexpr explicit u128(const u32 value)
        : low(value),
          high(0)
    {}

    constexpr explicit u128(const u64 value)
        : low(value),
          high(0)
    {}

    constexpr u128(const u64 low_value, const u64 high_value)
        : low(low_value),
          high(high_value)
    {}

    explicit inline operator u32() const
    {
        return static_cast<u32>(low);
    }

    explicit inline operator u64() const
    {
        return low;
    }
};

#define BEE_PRIxu128 "016" PRIx64 "%016" PRIx64

#define BEE_FMT_u128(value) value.high, value.low

/*
 * Numeric limits - also includes float max and double max as they're often used in the same places
 */
namespace limits {


template <typename T>
inline constexpr T max();

template <typename T>
inline constexpr T min();

/*
 * Signed types i8 -> i64
 */

// i8
template <>
inline constexpr i8 max()
{
    return INT8_MAX;
}

template <>
inline constexpr i8 min()
{
    return INT8_MIN;
}

// i16
template <>
inline constexpr i16 max()
{
    return INT16_MAX;
}

template <>
inline constexpr i16 min()
{
    return INT16_MIN;
}

// i32
template <>
inline constexpr i32 max()
{
    return INT32_MAX;
}

template <>
inline constexpr i32 min()
{
    return INT32_MIN;
}

// i64
template <>
inline constexpr i64 max()
{
    return INT64_MAX;
}

template <>
inline constexpr i64 min()
{
    return INT64_MIN;
}

/*
 * Unigned types u8 -> u64
 */

// u8
template <>
inline constexpr u8 max()
{
    return UINT8_MAX;
}

template <>
inline constexpr u8 min()
{
    return 0;
}

// u16
template <>
inline constexpr u16 max()
{
    return UINT16_MAX;
}

template <>
inline constexpr u16 min()
{
    return 0;
}

// u32
template <>
inline constexpr u32 max()
{
    return UINT32_MAX;
}

template <>
inline constexpr u32 min()
{
    return 0;
}

// u64
template <>
inline constexpr u64 max()
{
    return UINT64_MAX;
}

template <>
inline constexpr u64 min()
{
    return 0;
}

/*
 * floating point types - signed/unsigned
 */

// float
template <>
inline constexpr float max()
{
    return 3.402823466e+38f;
}

template <>
inline constexpr float min()
{
    return 1.175494351e-38f;
}

// double
template <>
inline constexpr double max()
{
    return 1.7976931348623158e+308;
}

template <>
inline constexpr double min()
{
    return 2.2250738585072014e-308;
}


} // limits


template <typename ResultType, typename ValueType>
BEE_FORCE_INLINE ResultType sign_cast(const ValueType& value)
{
BEE_PUSH_WARNING
BEE_DISABLE_WARNING_MSVC(4018) // signed/unsigned mismatch
    BEE_ASSERT(value >= 0);
    BEE_ASSERT(value <= limits::max<ResultType>());
BEE_POP_WARNING
    return static_cast<ResultType>(value);
}


} // namespace bee
