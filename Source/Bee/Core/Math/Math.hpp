//
//  Math.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 3/06/2018
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

/*
 * TODO(Jacob):
 *  - frustum
 *  - infinite_persepctive
 *  - fix ortho
 *  - matrix inverse
 *  - determinant
 *  - quaternions
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {
namespace math {

/*
 * Constants
 */
constexpr float pi = 3.14159265358979323846264338327950288f;

constexpr float two_pi = 2.0f * pi;

constexpr float half_pi = 0.5f * pi;

constexpr float float_epsilon = 1.19209290E-07f;

/*
 * Forward declared cmath wrappers
 */

BEE_API double sqrt(double value);

BEE_API float sqrtf(float value);

BEE_API double pow(double base, double exponent);

BEE_API double powf(float base, float exponent);

BEE_API double floor(double value);
BEE_API float floorf(float value);

BEE_API double ceil(double value);
BEE_API float ceilf(float value);

BEE_API float acosf(float value);
BEE_API double acos(double value);

BEE_API float asinf(float value);
BEE_API double asin(double value);

BEE_API float atanf(float value);
BEE_API double atan(double value);

BEE_API float atan2f(float y, float x);
BEE_API double atan2(double y, double x);

BEE_API float cosf(float value);
BEE_API double cos(double value);

BEE_API float sinf(float value);
BEE_API double sin(double value);

BEE_API float tanf(float value);
BEE_API double tan(double value);

BEE_API float acoshf(float value);
BEE_API double acosh(double value);

BEE_API float asinhf(float value);
BEE_API double asinh(double value);

BEE_API float atanhf(float value);
BEE_API double atanh(double value);

BEE_API float coshf(float value);
BEE_API double cosh(double value);

BEE_API float sinhf(float value);
BEE_API double sinh(double value);

BEE_API float tanhf(float value);
BEE_API double tanh(double value);

BEE_API double abs(double value);
BEE_API float fabs(float value);
BEE_API i32 iabs(i32 value);

/*
 * inline math operations
 */

template <typename T>
BEE_FORCE_INLINE const T& clamp(const T& value, const T& low, const T& high)
{
    return value < low ? low : (value > high ? high : value);
}

template <typename T>
BEE_FORCE_INLINE T rad_to_deg(const T& rad)
{
    return rad * (static_cast<T>(180) / pi);
}

template <typename T>
BEE_FORCE_INLINE T deg_to_rad(const T& deg)
{
    return deg * (pi / static_cast<T>(180));
}

template <typename T>
BEE_FORCE_INLINE const T& max(const T& a, const T& b)
{
    return (a > b) ? a : b;
}

template <typename T>
BEE_FORCE_INLINE const T& min(const T& a, const T& b)
{
    return (a < b) ? a : b;
}

constexpr bool is_power_of_two(const u32 num) noexcept
{
    return (num && (num & (num - 1)) == 0) || num == 1;
}

// source: https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
constexpr u32 to_next_pow2(const u32 value)
{
    auto result = value - 1;
    result |= result >> 1;
    result |= result >> 2;
    result |= result >> 4;
    result |= result >> 8;
    result |= result >> 16;
    return result + 1;
}

// source: https://graphics.stanford.edu/~seander/bithacks.html#IntegerLogDeBruijn
BEE_FORCE_INLINE u32 log2i(const u32 value)
{
    static constexpr u32 table[] = {
        0,  9,  1, 10, 13, 21,  2, 29,
        11, 14, 16, 18, 22, 25,  3, 30,
        8, 12, 20, 28, 15, 17, 24,  7,
        19, 27, 23,  6, 26,  5,  4, 31
    };

    auto result = value;
    result |= result >> 1;
    result |= result >> 2;
    result |= result >> 4;
    result |= result >> 8;
    result |= result >> 16;

    return table[(result * 0x07C4ACDDU) >> 27];
}

template <typename LerpType, typename T>
BEE_FORCE_INLINE LerpType lerp(const LerpType& a, const LerpType& b, const T& t)
{
    return a + t * (b - a);
}

BEE_FORCE_INLINE bool approximately_equal(float a, float b, float epsilon = float_epsilon)
{
    return fabs(a - b) <= epsilon;
}

} // namespace math
} // namespace bee
