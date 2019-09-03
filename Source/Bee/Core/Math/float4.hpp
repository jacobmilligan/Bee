//
//  float4.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 15/09/2018
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#pragma once

#include "Bee/Core/Math/float3.hpp"

namespace bee {


struct float4 : public vec<float, 4> {
    union {
        struct {
            value_t x, y, z, w;
        };

        struct {
            value_t r, g, b, a;
        };

        value_t components[num_components];
    };

    BEE_FORCE_INLINE float4() // NOLINT(cppcoreguidelines-pro-type-member-init)
    : x(0.0f), y(0.0f), z(0.0f), w(0.0f)
    {}

    BEE_FORCE_INLINE explicit float4(const value_t value) // NOLINT(cppcoreguidelines-pro-type-member-init)
    : x(value), y(value), z(value), w(value)
    {}

    BEE_FORCE_INLINE float4(const value_t cx, const value_t cy, const value_t cz, const value_t cw) // NOLINT(cppcoreguidelines-pro-type-member-init)
    : x(cx), y(cy), z(cz), w(cw)
    {}

    BEE_FORCE_INLINE explicit float4(const float2& cvec) // NOLINT(cppcoreguidelines-pro-type-member-init)
        : x(cvec.x), y(cvec.y), z(0.0f), w(0.0f)
    {}

    BEE_FORCE_INLINE float4(const float2& cvec, const value_t cz, const value_t cw) // NOLINT(cppcoreguidelines-pro-type-member-init)
    : x(cvec.x), y(cvec.y), z(cz), w(cw)
    {}

    BEE_FORCE_INLINE explicit float4(const float3& cvec) // NOLINT(cppcoreguidelines-pro-type-member-init)
        : x(cvec.x), y(cvec.y), z(cvec.z), w(0.0f)
    {}

    BEE_FORCE_INLINE float4(const float3& cvec, const value_t cw) // NOLINT(cppcoreguidelines-pro-type-member-init)
    : x(cvec.x), y(cvec.y), z(cvec.z), w(cw)
    {}

    BEE_FORCE_INLINE const value_t& operator[](const uint32_t i) const
    {
        return components[i];
    }
};

/////////////////////////
/// float4 - Operators
////////////////////////

/// Adds the components of two vectors together
BEE_FORCE_INLINE float4 operator+(const float4& left, const float4& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w };
}

/// Adds each component of a vector to a scalar
BEE_FORCE_INLINE float4 operator+(const float4& left, const float4::value_t right)
{
    return { left.x + right, left.y + right, left.z + right, left.w + right };
}

/// Subtracts the components of two vectors from one another
BEE_FORCE_INLINE float4 operator-(const float4& left, const float4& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w };
}

/// Subtracts a scalar value from each component of a vector
BEE_FORCE_INLINE float4 operator-(const float4& left, const float4::value_t right)
{
    return { left.x - right, left.y - right, left.z - right, left.w - right };
}

/// Multiplies the components of two vectors together
BEE_FORCE_INLINE float4 operator*(const float4& left, const float4& right)
{
    return { left.x * right.x, left.y * right.y, left.z * right.z, left.w * right.w };
}

/// Multiplies the components of a vector with a scalar value
BEE_FORCE_INLINE float4 operator*(const float4& left, const float4::value_t right)
{
    return { left.x * right, left.y * right, left.z * right, left.w * right };
}

/// Multiplies the components of a vector with a scalar value
BEE_FORCE_INLINE float4 operator*(const float4::value_t left, const float4& right)
{
    return { right.x * left, right.y * left, right.z * left, right.w * left };
}

/// Divides the components of a vector by the components of another vector
BEE_FORCE_INLINE float4 operator/(const float4& left, const float4& right)
{
    return { left.x / right.x, left.y / right.y, left.z / right.z, left.w / right.w };
}

/// Divides the components of a vector by a scalar value
BEE_FORCE_INLINE float4 operator/(const float4& left, const float4::value_t right)
{
    return { left.x / right, left.y / right, left.z / right, left.w / right };
}

/// Negates the components of a vector
BEE_FORCE_INLINE float4 operator-(const float4& vec)
{
    return { -vec.x, -vec.y, -vec.z, -vec.w };
}

BEE_FORCE_INLINE float4& operator+=(float4& left, const float4& right)
{
left = left + right;
return left;
}

BEE_FORCE_INLINE float4& operator+=(float4& left, const float4::value_t right)
{
    left = left + right;
    return left;
}

BEE_FORCE_INLINE float4& operator-=(float4& left, const float4& right)
{
    left = left - right;
    return left;
}

BEE_FORCE_INLINE float4& operator-=(float4& left, const float4::value_t right)
{
    left = left - right;
    return left;
}

BEE_FORCE_INLINE float4& operator*=(float4& left, const float4& right)
{
    left = left * right;
    return left;
}

BEE_FORCE_INLINE float4& operator*=(float4& left, const float4::value_t right)
{
    left = left * right;
    return left;
}

BEE_FORCE_INLINE float4& operator/=(float4& left, const float4& right)
{
    left = left / right;
    return left;
}

BEE_FORCE_INLINE float4& operator/=(float4& left, const float4::value_t right)
{
    left = left / right;
    return left;
}

/// Checks if two vectors are equivalent
BEE_FORCE_INLINE bool operator==(const float4& left, const float4& right)
{
    return math::approximately_equal(left.x, right.x)
        && math::approximately_equal(left.y, right.y)
        && math::approximately_equal(left.z, right.z)
        && math::approximately_equal(left.w, right.w);
}

/// Checks if two vectors are not equivalent
BEE_FORCE_INLINE bool operator!=(const float4& left, const float4& right)
{
    return !(left == right);
}

/////////////////////////
/// float4 - Functions
////////////////////////

/// Computes the dot product of two vectors
BEE_FORCE_INLINE float4::value_t dot(const float4& left, const float4& right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

/// Computes the length of a vector
BEE_FORCE_INLINE float4::value_t length(const float4& vec)
{
    return math::sqrtf((vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z));
}

/// Computes squared length of a vector
BEE_FORCE_INLINE float4::value_t squared_length(const float4& vec)
{
    return (vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z);
}

/// Normalizes a vector and returns it's result
BEE_FORCE_INLINE float4 normalize(const float4& vec)
{
    const auto len = length(vec);
    const auto n = 1.0f / (len <= 0 ? 1.0f : len);
    return { vec.x * n, vec.y * n, vec.z * n, vec.w * n };
}

/// Clamps a vector to a lower and upper bound and returns its result
BEE_FORCE_INLINE float4 clamp(const float4& vec, const float4& lower, const float4& upper)
{
    return {
        math::clamp(vec.x, lower.x, upper.x),
        math::clamp(vec.y, lower.y, upper.y),
        math::clamp(vec.z, lower.z, upper.z),
        math::clamp(vec.w, lower.w, upper.w)
    };
}

/// Clamps a vector to a lower and upper bound and returns its result
BEE_FORCE_INLINE float4 clamp(const float4& vec, const float4::value_t lower, const float4::value_t upper)
{
    return {
        math::clamp(vec.x, lower, upper),
        math::clamp(vec.y, lower, upper),
        math::clamp(vec.z, lower, upper),
        math::clamp(vec.w, lower, upper)
    };
}

/// Computes the distance between a vector and its target
BEE_FORCE_INLINE float4::value_t distance(const float4& vec, const float4& target)
{
    auto x_dist = vec.x - target.x;
    auto y_dist = vec.y - target.y;
    auto z_dist = vec.z - target.z;
    auto w_dist = vec.w - target.w;
    return math::sqrtf(x_dist * x_dist + y_dist * y_dist + z_dist * z_dist + w_dist * w_dist);
}

BEE_FORCE_INLINE float4 floor(const float4& vec)
{
    return { math::floorf(vec.x), math::floorf(vec.y), math::floorf(vec.z), math::floorf(vec.w) };
}

BEE_FORCE_INLINE float4 mod(const float4& numer, const float4& denom)
{
    return numer - denom * floor(numer / denom);
}

BEE_FORCE_INLINE float4 max(const float4& lhs, const float4& rhs)
{
    return float4(math::max(lhs.x, rhs.x), math::max(lhs.y, rhs.y), math::max(lhs.z, rhs.z), math::max(lhs.w, rhs.w));
}

BEE_FORCE_INLINE float4 min(const float4& lhs, const float4& rhs)
{
    return float4(math::min(lhs.x, rhs.x), math::min(lhs.y, rhs.y), math::min(lhs.z, rhs.z), math::min(lhs.w, rhs.w));
}


} // namespace bee