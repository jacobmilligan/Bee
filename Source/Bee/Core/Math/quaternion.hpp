/*
 *  quaternion.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Math/float4x4.hpp"

namespace bee {


float dot(const quaternion& left, const quaternion& right);

float squared_length(const quaternion& quat);

float length(const quaternion& quat);

quaternion normalize(const quaternion& quat);

struct quaternion
{
    static constexpr uint32_t num_components = 4;

    union
    {
        struct
        {
            float w, x, y, z;
        };

        float components[num_components];
    };

    BEE_FORCE_INLINE quaternion() noexcept // NOLINT(cppcoreguidelines-pro-type-member-init)
        : w(1.0f), x(0.0f), y(0.0f), z(0.0f)
    {}

    BEE_FORCE_INLINE quaternion(const float in_w, const float in_x, // NOLINT(cppcoreguidelines-pro-type-member-init)
                                const float in_y, const float in_z) noexcept
        : w(in_w), x(in_x), y(in_y), z(in_z)
    {}

    explicit BEE_FORCE_INLINE quaternion(const float4& vec) noexcept // NOLINT(cppcoreguidelines-pro-type-member-init)
        : w(vec.w), x(vec.x), y(vec.y), z(vec.z)
    {}

    explicit BEE_FORCE_INLINE quaternion(const float4x4& mat) noexcept // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        // adapted from: http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToquaternion/

        const auto trace = mat.m00 + mat.m11 + mat.m22;

        // compute the matrix trace - if greater than 0, reverse compute matrix normally
        if (trace > 0.0f)
        {
            w = math::sqrtf(trace + 1.0f) * 0.5f;
            const auto s = 0.25f / w;
            x = (mat.m21 - mat.m12) * s;
            y = (mat.m02 - mat.m20) * s;
            z = (mat.m10 - mat.m01) * s;
        }
        else if (mat.m00 > mat.m11 && mat.m00 > mat.m22)
        {
            // otherwise, find the major diagonal with the greatest value and use that to compute the quat
            // test m00:
            x = math::sqrtf(mat.m00 - mat.m11 - mat.m22 + 1.0f) * 0.5f;
            const auto s = 0.25f / x;
            w = (mat.m21 - mat.m12) * s;
            y = (mat.m10 + mat.m01) * s;
            z = (mat.m02 + mat.m20) * s;
        }
        else if (mat.m11 > mat.m22)
        {
            // test m11:
            y = math::sqrtf(mat.m11 - mat.m00 - mat.m22 + 1.0f) * 0.5f;
            const auto s = 0.25f / y;
            w = (mat.m02 - mat.m20) * s;
            x = (mat.m10 + mat.m01) * s;
            z = (mat.m21 + mat.m12) * s;
        }
        else
        {
            // m22 is greatest
            z = math::sqrtf(mat.m22 - mat.m00 - mat.m11 + 1.0f) * 0.5f;
            const auto s = 0.25f / z;
            w = (mat.m10 - mat.m01) * s;
            x = (mat.m02 + mat.m20) * s;
            y = (mat.m21 + mat.m12) * s;
        }

        *this = normalize(*this);
    }

    BEE_API static const quaternion& identity();
};

///////////////////////////
/// quaternion - operators
///////////////////////////

BEE_FORCE_INLINE quaternion operator*(const quaternion& left, const quaternion& right)
{
    return {
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y + left.y * right.w + left.z * right.x - left.x * right.z,
        left.w * right.z + left.z * right.w + left.x * right.y - left.y * right.x
    };
}

BEE_FORCE_INLINE quaternion operator*(const quaternion& quat, const float& scalar)
{
    return { quat.w * scalar, quat.x * scalar, quat.y * scalar, quat.z * scalar };
}

BEE_FORCE_INLINE float3 operator*(const quaternion& quat, const float3& vec)
{
    const float3 quat_vec_part(quat.x, quat.y, quat.z);
    const auto qv_vec = cross(quat_vec_part, vec);
    const auto qv_qv_vec = cross(quat_vec_part, qv_vec);

    return vec + ((qv_vec * quat.w) + qv_qv_vec) * 2.0f;
}

BEE_FORCE_INLINE quaternion operator-(const quaternion& quat)
{
    return { -quat.w, -quat.x, -quat.y, -quat.z };
}

BEE_FORCE_INLINE bool operator==(const quaternion& left, const quaternion& right)
{
    return math::approximately_equal(left.w, right.w)
        && math::approximately_equal(left.x, right.x)
        && math::approximately_equal(left.y, right.y)
        && math::approximately_equal(left.z, right.z);
}

BEE_FORCE_INLINE bool operator!=(const quaternion& left, const quaternion& right)
{
    return !(left == right);
}

///////////////////////////
/// quaternion - functions
///////////////////////////

BEE_FORCE_INLINE float dot(const quaternion& left, const quaternion& right)
{
    return left.w * right.w + left.x * right.x + left.y * right.y + left.z * right.z;
}

BEE_FORCE_INLINE float squared_length(const quaternion& quat)
{
    return dot(quat, quat);
}

BEE_FORCE_INLINE float length(const quaternion& quat)
{
    return math::sqrtf(dot(quat, quat));
}

BEE_FORCE_INLINE quaternion normalize(const quaternion& quat)
{
    const auto quat_len = length(quat);
    if (quat_len <= 0.0f)
    {
        // invalid length - return identity quat
        return quaternion::identity();
    }

    const auto one_over_len = 1.0f / quat_len;
    return { quat.w * one_over_len, quat.x * one_over_len, quat.y * one_over_len, quat.z * one_over_len};
}

BEE_FORCE_INLINE quaternion conjugate(const quaternion& quat)
{
    return { quat.w, -quat.x, -quat.y, -quat.z };
}

BEE_FORCE_INLINE quaternion inverse(const quaternion& quat)
{
    BEE_ASSERT_F(math::approximately_equal(squared_length(quat), 1.0f),
               "Getting the inverse of an unnormalized quaternion is an undefined operation");

    return conjugate(quat);
}

BEE_FORCE_INLINE quaternion slerp(const quaternion& a, const quaternion& b, const float t)
{
    auto temp = b;
    auto cos_omega = dot(a, temp);
    // ensure we take the shortest path around the 4d arc
    if (cos_omega < 0.0f)
    {
        temp = -temp;
        cos_omega = -cos_omega;
    }

    float k0 = 0.0f;
    float k1 = 0.0f;

    // use a regular linear interpolation if the quaternions are super close, otherwise compute
    // regular SLERP
    if (cos_omega > 1.0f - math::float_epsilon)
    {
        k0 = 1.0f - t;
        k1 = t;
    }
    else
    {
        const auto sin_omega = math::sqrtf(1.0f - cos_omega * cos_omega);
        const auto omega = math::atan2f(sin_omega, cos_omega);
        const auto one_over_sin_omega = 1.0f / sin_omega;

        k0 = math::sinf((1.0f - t) * omega) * one_over_sin_omega;
        k1 = math::sinf(t * omega) * one_over_sin_omega;
    }

    // interpolate components
    return
    {
        a.w * k0 + temp.w * k1,
        a.x * k0 + temp.x * k1,
        a.y * k0 + temp.y * k1,
        a.z * k0 + temp.z * k1
    };
}

BEE_FORCE_INLINE quaternion nlerp(const quaternion& a, const quaternion& b, const float t)
{
    const auto scale_a = 1.0f - t;
    const auto scale_b = (dot(a, b) >= 0.0f) ? t : -t;
    const auto result = quaternion(scale_a * a.w + scale_b * b.w,
                                   scale_a * a.x + scale_b * b.x,
                                   scale_a * a.y + scale_b * b.y,
                                   scale_a * a.z + scale_b * b.z);

    return normalize(result);
}

BEE_FORCE_INLINE quaternion axis_angle(const float3& axis, const float angle)
{
    const auto sin_half_angle = math::sinf(angle * 0.5f);
    const auto cos_half_angle = math::cosf(angle * 0.5f);

    return
    {
        cos_half_angle,
        axis.x * sin_half_angle,
        axis.y * sin_half_angle,
        axis.z * sin_half_angle
    };
}

BEE_FORCE_INLINE quaternion make_rotation(const float3& from, const float3& to)
{
    const auto from_n = normalize(from);
    const auto to_n = normalize(to);
    const auto cos_theta = dot(from_n, to_n);

    // the case in which the vectors are pointing in the same direction
    if (cos_theta >= 1.0f - math::float_epsilon)
    {
        return quaternion::identity();
    }

    if (cos_theta < math::float_epsilon - 1.0f)
    {
        // vectors in opposite directions - no ideal rotation so use up vector
        auto rotation_axis = cross({0.0f, 1.0f, 0.0f}, from_n);
        if (squared_length(rotation_axis) < math::float_epsilon)
        {
            // up & from_n were parallel so try with a different axis
            rotation_axis = cross({1.0f, 0.0f, 0.0f}, from_n);
        }
        rotation_axis = normalize(rotation_axis);
        return axis_angle(rotation_axis, math::pi);
    }

    const auto rotation_axis = cross(from_n, to_n);
    const auto sqrt_two_times_one_plus_cos_theta = math::sqrtf((1.0f + cos_theta) * 2.0f);
    const auto inverse_sqrt = 1.0f / sqrt_two_times_one_plus_cos_theta;

    quaternion result(sqrt_two_times_one_plus_cos_theta * 0.5f,
                      rotation_axis.x * inverse_sqrt,
                      rotation_axis.y * inverse_sqrt,
                      rotation_axis.z * inverse_sqrt);
    return normalize(result);
}

BEE_FORCE_INLINE quaternion look_rotation(const float3& direction, const float3& up)
{
    if (squared_length(direction) <= math::float_epsilon)
    {
        return quaternion::identity();
    }

    const auto dir_n = normalize(direction);
    const auto right = normalize(cross(up, dir_n));
    const auto up_perp = cross(dir_n, right);

    return quaternion(float4x4(float4(right), float4(up_perp), float4(dir_n), float4(0.0f)));
}

template <typename PtrType>
BEE_FORCE_INLINE quaternion make_quat_from_ptr(const PtrType* ptr)
{
    static_assert(sizeof(PtrType) <= sizeof(float),
        "make_quat_from_ptr: sizeof(PtrType) must be <= sizeof(float) "
        "otherwise a narrowing conversion would occur resulting in a loss of, or incorrect, data");

    quaternion result;
    memcpy(&result.components, ptr, sizeof(float) * 4);
    return result;
}

template <typename PtrType>
BEE_FORCE_INLINE quaternion make_quat_from_ptr_xyzw(const PtrType* ptr)
{
    static_assert(sizeof(PtrType) <= sizeof(float),
        "make_quat_from_ptr_xyzw: sizeof(PtrType) must be <= sizeof(float) "
        "otherwise a narrowing conversion would occur resulting in a loss of, or incorrect, data");

    const auto xyz_byte_len = sizeof(float) * 3;

    quaternion result;
    memcpy(&result.components[1], ptr, xyz_byte_len);
    memcpy(&result, ptr + xyz_byte_len, sizeof(float));
    return result;
}


} // namespace bee