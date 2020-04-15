/*
 *  float2.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Math/vec.hpp"

namespace bee {


struct BEE_REFLECT(serializable) float2 : public vec<float, 2> {
    union BEE_REFLECT(serializable)
    {
        BEE_REFLECT(nonserialized)
        component_array_t components;

        struct BEE_REFLECT(serializable)
        {
            value_t x, y;
        };

        /*
         * Non-reflected aliases for x, y
         */
        struct
        {
            value_t u, v;
        };
    };

    float2() noexcept
    : x(0.0f), y(0.0f)
    {}

    BEE_FORCE_INLINE explicit float2(const value_t value) noexcept
    : x(value), y(value)
    {}

    BEE_FORCE_INLINE float2(const value_t cx, const value_t cy) noexcept
    : x(cx), y(cy)
    {}

    BEE_FORCE_INLINE value_t operator[](const u32 i)
    {
        return components[i];
    }
};

/////////////////////////
/// float2 - Operators
////////////////////////

/// Adds the components of two vectors together
BEE_FORCE_INLINE float2 operator+(const float2& left, const float2& right)
{
    return { left.x + right.x, left.y + right.y };
}

/// Adds each component of a vector to a scalar
BEE_FORCE_INLINE float2 operator+(const float2& left, const float2::value_t right)
{
    return { left.x + right, left.y + right };
}

/// Subtracts the components of two vectors from one another
BEE_FORCE_INLINE float2 operator-(const float2& left, const float2& right)
{
    return { left.x - right.x, left.y - right.y };
}

/// Subtracts a scalar value from each component of a vector
BEE_FORCE_INLINE float2 operator-(const float2& left, const float2::value_t right)
{
    return { left.x - right, left.y - right };
}

/// Multiplies the components of two vectors together
BEE_FORCE_INLINE float2 operator*(const float2& left, const float2& right)
{
    return { left.x * right.x, left.y * right.y };
}

/// Multiplies the components of a vector with a scalar value
BEE_FORCE_INLINE float2 operator*(const float2& left, const float2::value_t right)
{
    return { left.x * right, left.y * right };
}

/// Multiplies the components of a vector with a scalar value
BEE_FORCE_INLINE float2 operator*(const float2::value_t left, const float2& right)
{
    return { right.x * left, right.y * left };
}

/// Divides the components of a vector by the components of another vector
BEE_FORCE_INLINE float2 operator/(const float2& left, const float2& right)
{
    return { left.x / right.x, left.y / right.y };
}

/// Divides the components of a vector by a scalar value
BEE_FORCE_INLINE float2 operator/(const float2& left, const float2::value_t right)
{
    return { left.x / right, left.y / right };
}

/// Negates the components of a vector
BEE_FORCE_INLINE float2 operator-(const float2& vec)
{
    return { -vec.x, -vec.y };
}

BEE_FORCE_INLINE float2& operator+=(float2& left, const float2& right)
{
    left = left + right;
    return left;
}

BEE_FORCE_INLINE float2& operator+=(float2& left, const float2::value_t right)
{
    left = left + right;
    return left;
}

BEE_FORCE_INLINE float2& operator-=(float2& left, const float2& right)
{
    left = left - right;
    return left;
}

BEE_FORCE_INLINE float2& operator-=(float2& left, const float2::value_t right)
{
    left = left - right;
    return left;
}

BEE_FORCE_INLINE float2& operator*=(float2& left, const float2& right)
{
    left = left * right;
    return left;
}

BEE_FORCE_INLINE float2& operator*=(float2& left, const float2::value_t right)
{
    left = left * right;
    return left;
}

BEE_FORCE_INLINE float2& operator/=(float2& left, const float2& right)
{
    left = left / right;
    return left;
}

BEE_FORCE_INLINE float2& operator/=(float2& left, const float2::value_t right)
{
    left = left / right;
    return left;
}

/// Checks if two vectors are equivalent
BEE_FORCE_INLINE bool operator==(const float2& left, const float2& right)
{
    return math::approximately_equal(left.x, right.x)
        && math::approximately_equal(left.y, right.y);
}

/// Checks if two vectors are not equivalent
BEE_FORCE_INLINE bool operator!=(const float2& left, const float2& right)
{
    return !(left == right);
}

/////////////////////////
/// float2 - Functions
////////////////////////

/// Computes the dot product of two vectors
BEE_FORCE_INLINE float2::value_t dot(const float2& left, const float2& right)
{
    return (left.x * right.x) + (left.y * right.y);
}

/// Computes the length of a vector
BEE_FORCE_INLINE float2::value_t length(const float2& vec)
{
    return math::sqrtf((vec.x * vec.x) + (vec.y * vec.y));
}

/// Computes squared length of a vector
BEE_FORCE_INLINE float2::value_t squared_length(const float2& vec)
{
    return (vec.x * vec.x) + (vec.y * vec.y);
}

/// Normalizes a vector and returns it's result
BEE_FORCE_INLINE float2 normalize(const float2& vec)
{
    const auto len = length(vec);
    const auto n = 1.0f / (len <= 0 ? 1.0f : len);
    return { vec.x * n, vec.y * n };
}

/// Clamps a vector to a lower and upper bound and returns its result
BEE_FORCE_INLINE float2 clamp(const float2& vec, const float2& lower, const float2& upper)
{
    return { math::clamp(vec.x, lower.x, upper.x), math::clamp(vec.y, lower.y, upper.y) };
}

/// Clamps a vector to a lower and upper bound and returns its result
BEE_FORCE_INLINE float2 clamp(const float2& vec, const float2::value_t lower, const float2::value_t upper)
{
    return { math::clamp(vec.x, lower, upper), math::clamp(vec.y, lower, upper) };
}

/// Computes the distance between a vector and its target
BEE_FORCE_INLINE float2::value_t distance(const float2& vec, const float2& target)
{
    auto x_dist = vec.x - target.x;
    auto y_dist = vec.y - target.y;
    return math::sqrtf(x_dist * x_dist + y_dist * y_dist);
}

BEE_FORCE_INLINE float2 floor(const float2& vec)
{
    return { math::floorf(vec.x), math::floorf(vec.y) };
}

BEE_FORCE_INLINE float2 mod(const float2& numer, const float2& denom)
{
    return numer - denom * floor(numer / denom);
}

BEE_FORCE_INLINE float2 max(const float2& lhs, const float2& rhs)
{
    return float2(math::max(lhs.x, rhs.x), math::max(lhs.y, rhs.y));
}

BEE_FORCE_INLINE float2 min(const float2& lhs, const float2& rhs)
{
    return float2(math::min(lhs.x, rhs.x), math::min(lhs.y, rhs.y));
}


} // namespace bee
