/*
 *  float3.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Math/float2.hpp"

namespace bee {


struct BEE_REFLECT(serializable) float3 : public vec<float, 3> {
    union BEE_REFLECT(serializable)
    {
        BEE_REFLECT(nonserialized)
        value_t components[num_components];

        struct BEE_REFLECT(serializable)
        {
            value_t x, y, z;
        };

        /*
         * Non-reflected aliases for x, y, z
         */
        struct
        {
            value_t r, g, b;
        };

        struct
        {
            value_t u, v, w;
        };
    };

    BEE_FORCE_INLINE float3() // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        : x(0.0f), y(0.0f), z(0.0f)
    {}

    BEE_FORCE_INLINE explicit float3(const value_t value) // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        : x(value), y(value), z(value)
    {}

    BEE_FORCE_INLINE float3(const value_t cx, const value_t cy, const value_t cz) // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        : x(cx), y(cy), z(cz)
    {}

    BEE_FORCE_INLINE explicit float3(const float2& cvec) // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        : x(cvec.x), y(cvec.y), z(0.0f)
    {}

    BEE_FORCE_INLINE float3(const float2& cvec, const value_t cz) // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        : x(cvec.x), y(cvec.y), z(cz)
    {}

    BEE_FORCE_INLINE value_t operator[](const uint32_t i)
    {
        return components[i];
    }
};

/////////////////////////
/// float3 - Operators
////////////////////////

/// Adds the components of two vectors together
BEE_FORCE_INLINE float3 operator+(const float3& left, const float3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

/// Adds each component of a vector to a scalar
BEE_FORCE_INLINE float3 operator+(const float3& left, const float3::value_t right)
{
    return { left.x + right, left.y + right, left.z + right };
}

/// Subtracts the components of two vectors from one another
BEE_FORCE_INLINE float3 operator-(const float3& left, const float3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

/// Subtracts a scalar value from each component of a vector
BEE_FORCE_INLINE float3 operator-(const float3& left, const float3::value_t right)
{
    return { left.x - right, left.y - right, left.z - right };
}

/// Multiplies the components of two vectors together
BEE_FORCE_INLINE float3 operator*(const float3& left, const float3& right)
{
    return { left.x * right.x, left.y * right.y, left.z * right.z };
}

/// Multiplies the components of a vector with a scalar value
BEE_FORCE_INLINE float3 operator*(const float3& left, const float3::value_t right)
{
    return { left.x * right, left.y * right, left.z * right };
}

/// Multiplies the components of a vector with a scalar value
BEE_FORCE_INLINE float3 operator*(const float3::value_t left, const float3& right)
{
    return { right.x * left, right.y * left, right.z * left };
}

/// Divides the components of a vector by the components of another vector
BEE_FORCE_INLINE float3 operator/(const float3& left, const float3& right)
{
    return { left.x / right.x, left.y / right.y, left.z / right.z };
}

/// Divides the components of a vector by a scalar value
BEE_FORCE_INLINE float3 operator/(const float3& left, const float3::value_t right)
{
    return { left.x / right, left.y / right, left.z / right };
}

/// Negates the components of a vector
BEE_FORCE_INLINE float3 operator-(const float3& vec)
{
    return { -vec.x, -vec.y, -vec.z };
}

BEE_FORCE_INLINE float3& operator+=(float3& left, const float3& right)
{
    left = left + right;
    return left;
}

BEE_FORCE_INLINE float3& operator+=(float3& left, const float3::value_t right)
{
    left = left + right;
    return left;
}

BEE_FORCE_INLINE float3& operator-=(float3& left, const float3& right)
{
    left = left - right;
    return left;
}

BEE_FORCE_INLINE float3& operator-=(float3& left, const float3::value_t right)
{
    left = left - right;
    return left;
}

BEE_FORCE_INLINE float3& operator*=(float3& left, const float3& right)
{
    left = left * right;
    return left;
}

BEE_FORCE_INLINE float3& operator*=(float3& left, const float3::value_t right)
{
    left = left * right;
    return left;
}

BEE_FORCE_INLINE float3& operator/=(float3& left, const float3& right)
{
    left = left / right;
    return left;
}

BEE_FORCE_INLINE float3& operator/=(float3& left, const float3::value_t right)
{
    left = left / right;
    return left;
}

/// Checks if two vectors are equivalent
BEE_FORCE_INLINE bool operator==(const float3& left, const float3& right)
{
    return math::approximately_equal(left.x, right.x)
        && math::approximately_equal(left.y, right.y)
        && math::approximately_equal(left.z, right.z);
}

/// Checks if two vectors are not equivalent
BEE_FORCE_INLINE bool operator!=(const float3& left, const float3& right)
{
    return !(left == right);
}

/////////////////////////
/// float3 - Functions
////////////////////////

/// Computes the dot product of two vectors
BEE_FORCE_INLINE float3::value_t dot(const float3& left, const float3& right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

/// Computes the length of a vector
BEE_FORCE_INLINE float3::value_t length(const float3& vec)
{
    return math::sqrtf((vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z));
}

/// Computes squared length of a vector
BEE_FORCE_INLINE float3::value_t squared_length(const float3& vec)
{
    return (vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z);
}

/// Normalizes a vector and returns it's result
BEE_FORCE_INLINE float3 normalize(const float3& vec)
{
    const auto len = length(vec);
    const auto n = 1.0f / (len <= 0 ? 1.0f : len);
    return { vec.x * n, vec.y * n, vec.z * n };
}

/// Clamps a vector to a lower and upper bound and returns its result
BEE_FORCE_INLINE float3 clamp(const float3& vec, const float3& lower, const float3& upper)
{
    return {
        math::clamp(vec.x, lower.x, upper.x),
        math::clamp(vec.y, lower.y, upper.y),
        math::clamp(vec.z, lower.z, upper.z)
    };
}

/// Clamps a vector to a lower and upper bound and returns its result
BEE_FORCE_INLINE float3 clamp(const float3& vec, const float3::value_t lower, const float3::value_t upper)
{
    return {
        math::clamp(vec.x, lower, upper),
        math::clamp(vec.y, lower, upper),
        math::clamp(vec.z, lower, upper)
    };
}

/// Computes the distance between a vector and its target
BEE_FORCE_INLINE float3::value_t distance(const float3& vec, const float3& target)
{
    const auto x_dist = vec.x - target.x;
    const auto y_dist = vec.y - target.y;
    const auto z_dist = vec.z - target.z;
    return math::sqrtf(x_dist * x_dist + y_dist * y_dist + z_dist * z_dist);
}

BEE_FORCE_INLINE float3 cross(const float3& left, const float3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

BEE_FORCE_INLINE float3 floor(const float3& vec)
{
    return { math::floorf(vec.x), math::floorf(vec.y), math::floorf(vec.z) };
}

BEE_FORCE_INLINE float3 mod(const float3& numer, const float3& denom)
{
    return numer - denom * floor(numer / denom);
}

BEE_FORCE_INLINE float3 max(const float3& lhs, const float3& rhs)
{
    return float3(math::max(lhs.x, rhs.x), math::max(lhs.y, rhs.y), math::max(lhs.z, rhs.z));
}

BEE_FORCE_INLINE float3 min(const float3& lhs, const float3& rhs)
{
    return float3(math::min(lhs.x, rhs.x), math::min(lhs.y, rhs.y), math::min(lhs.z, rhs.z));
}


} // namespace bee
