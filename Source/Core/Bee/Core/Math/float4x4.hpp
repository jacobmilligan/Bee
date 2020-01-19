/*
 *  float4x4.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Math/float4.hpp"

namespace bee {

struct quaternion;

struct float4x4 {
    static constexpr uint8_t num_elements = 16;

    union
    {
        float entries[num_elements]{0.0f};

        struct
        {
            float m00, m01, m02, m03;
            float m10, m11, m12, m13;
            float m20, m21, m22, m23;
            float m30, m31, m32, m33;
        };
    };

    float4x4()
        : float4x4(0.0f)
    {}

    explicit float4x4(const float value)
    {
        m00 = value;
        m01 = 0.0f;
        m02 = 0.0f;
        m03 = 0.0f;
        m10 = 0.0f;
        m11 = value;
        m12 = 0.0f;
        m13 = 0.0f;
        m20 = 0.0f;
        m21 = 0.0f;
        m22 = value;
        m23 = 0.0f;
        m30 = 0.0f;
        m31 = 0.0f;
        m32 = 0.0f;
        m33 = value;
    }

    float4x4(const float in_m00, const float in_m01, const float in_m02, const float in_m03,
              const float in_m10, const float in_m11, const float in_m12, const float in_m13,
              const float in_m20, const float in_m21, const float in_m22, const float in_m23,
              const float in_m30, const float in_m31, const float in_m32, const float in_m33)
    {
        m00 = in_m00;
        m01 = in_m01;
        m02 = in_m02;
        m03 = in_m03;
        m10 = in_m10;
        m11 = in_m11;
        m12 = in_m12;
        m13 = in_m13;
        m20 = in_m20;
        m21 = in_m21;
        m22 = in_m22;
        m23 = in_m23;
        m30 = in_m30;
        m31 = in_m31;
        m32 = in_m32;
        m33 = in_m33;
    }

    float4x4(const float4& c0, const float4& c1, const float4& c2, const float4& c3)
    {
        m00 = c0.x;
        m10 = c0.y;
        m20 = c0.z;
        m30 = c0.w;
        m01 = c1.x;
        m11 = c1.y;
        m21 = c1.z;
        m31 = c1.w;
        m02 = c2.x;
        m12 = c2.y;
        m22 = c2.z;
        m32 = c2.w;
        m03 = c3.x;
        m13 = c3.y;
        m23 = c3.z;
        m33 = c3.w;
    }

    BEE_CORE_API explicit float4x4(const quaternion& quat);

    BEE_FORCE_INLINE float& operator[](const uint32_t i)
    {
        return entries[i];
    }

    BEE_FORCE_INLINE const float& operator[](const uint32_t i) const
    {
        return entries[i];
    }
};


///////////////////////////
/// float4x4 - Operators
//////////////////////////

BEE_FORCE_INLINE bool operator==(const float4x4& left, const float4x4& right)
{
    for (int elem_idx = 0; elem_idx < float4x4::num_elements; ++elem_idx)
    {
        if (!math::approximately_equal(left[elem_idx], right[elem_idx]))
        {
            return false;
        }
    }

    return true;
}

BEE_FORCE_INLINE bool operator!=(const float4x4& left, const float4x4& right)
{
    return !(left == right);
}


BEE_FORCE_INLINE float4x4 operator*(const float4x4& left, const float4x4& right)
{
    return {
        left.m00 * right.m00 + left.m01 * right.m10 + left.m02 * right.m20 + left.m03 * right.m30,
        left.m00 * right.m01 + left.m01 * right.m11 + left.m02 * right.m21 + left.m03 * right.m31,
        left.m00 * right.m02 + left.m01 * right.m12 + left.m02 * right.m22 + left.m03 * right.m32,
        left.m00 * right.m03 + left.m01 * right.m13 + left.m02 * right.m23 + left.m03 * right.m33,

        left.m10 * right.m00 + left.m11 * right.m10 + left.m12 * right.m20 + left.m13 * right.m30,
        left.m10 * right.m01 + left.m11 * right.m11 + left.m12 * right.m21 + left.m13 * right.m31,
        left.m10 * right.m02 + left.m11 * right.m12 + left.m12 * right.m22 + left.m13 * right.m32,
        left.m10 * right.m03 + left.m11 * right.m13 + left.m12 * right.m23 + left.m13 * right.m33,

        left.m20 * right.m00 + left.m21 * right.m10 + left.m22 * right.m20 + left.m23 * right.m30,
        left.m20 * right.m01 + left.m21 * right.m11 + left.m22 * right.m21 + left.m23 * right.m31,
        left.m20 * right.m02 + left.m21 * right.m12 + left.m22 * right.m22 + left.m23 * right.m32,
        left.m20 * right.m03 + left.m21 * right.m13 + left.m22 * right.m23 + left.m23 * right.m33,

        left.m30 * right.m00 + left.m31 * right.m10 + left.m32 * right.m20 + left.m33 * right.m30,
        left.m30 * right.m01 + left.m31 * right.m11 + left.m32 * right.m21 + left.m33 * right.m31,
        left.m30 * right.m02 + left.m31 * right.m12 + left.m32 * right.m22 + left.m33 * right.m32,
        left.m30 * right.m03 + left.m31 * right.m13 + left.m32 * right.m23 + left.m33 * right.m33
    };
}

BEE_FORCE_INLINE float4 operator*(const float4x4& mat, const float4& vec)
{
    return {
        mat.m00 * vec.x + mat.m10 * vec.y + mat.m20 * vec.z + mat.m30 * vec.w,
        mat.m01 * vec.x + mat.m11 * vec.y + mat.m21 * vec.z + mat.m31 * vec.w,
        mat.m02 * vec.x + mat.m12 * vec.y + mat.m22 * vec.z + mat.m32 * vec.w,
        mat.m03 * vec.x + mat.m13 * vec.y + mat.m23 * vec.z + mat.m33 * vec.w
    };
}

BEE_FORCE_INLINE float4x4& operator*=(float4x4& left, const float4x4& right)
{
    left = left * right;
    return left;
}

///////////////////////////
/// float4x4 - Functions
//////////////////////////

BEE_FORCE_INLINE float4x4 scale(const float3& svec)
{
    float4x4 result(1.0f);
    result.m00 = svec.x;
    result.m11 = svec.y;
    result.m22 = svec.z;
    return result;
}

BEE_FORCE_INLINE float4x4 translate(const float3& tvec)
{
    float4x4 result(1.0f);
    result.m30 = tvec.x;
    result.m31 = tvec.y;
    result.m32 = tvec.z;
    return result;
}

BEE_FORCE_INLINE float4x4 transpose(const float4x4& mat)
{
    float4x4 result(0.0f);
    result.m00 = mat.m00;
    result.m10 = mat.m01;
    result.m20 = mat.m02;
    result.m30 = mat.m03;
    result.m01 = mat.m10;
    result.m11 = mat.m11;
    result.m21 = mat.m12;
    result.m31 = mat.m13;
    result.m02 = mat.m20;
    result.m12 = mat.m21;
    result.m22 = mat.m22;
    result.m32 = mat.m23;
    result.m03 = mat.m30;
    result.m13 = mat.m31;
    result.m23 = mat.m32;
    result.m33 = mat.m33;
    return result;
}

BEE_FORCE_INLINE float4x4 ortho(const float left, const float right, const float bottom,
                                         const float top, const float near, const float far)
{
    float4x4 result;
    const auto size_x = right - left;
    const auto size_y = top - bottom;
    const auto zoom = far - near;

    result.m00 = 2.0f / size_x;
    result.m11 = 2.0f / size_y;
    result.m22 = -2.0f / zoom;
    result.m30 = -(right + left) / size_x;
    result.m31 = -(top + bottom) / size_y;
    result.m32 = -(far + near) / zoom;
    result.m33 = 1.0f;

    return result;
}

BEE_FORCE_INLINE float4x4 perspective(const float fov_y, const float aspect,
                                               const float z_near, const float z_far)
{
    // left-handed, zero to one
    float4x4 result;

    const auto inv_cotangent = 1.0f / math::tanf(fov_y * 0.5f);
    const auto focal_range = z_far / (z_far - z_near);

    result.m00 = inv_cotangent / aspect;
    result.m11 = inv_cotangent;
    result.m22 = focal_range;
    result.m23 = 1.0f;
    result.m32 = -z_near * focal_range;

    return result;
}

BEE_FORCE_INLINE float4x4 look_at(const float3& eye, const float3& target, const float3& up)
{
    // Zero to one, left-handed
    float4x4 result;

    const auto eye_dir = normalize(target - eye);
    const auto s = normalize(cross(up, eye_dir));
    const auto u = cross(eye_dir, s); // pointing up

    // Up
    result.m00 = s.x; // xaxis.x
    result.m01 = u.x; // xaxis.y
    result.m02 = eye_dir.x; // xaxis.z
    result.m10 = s.y; // yaxis.x
    result.m11 = u.y; // yaxis.y
    result.m12 = eye_dir.y; // yaxis.z
    result.m20 = s.z; // zaxis.x
    result.m21 = u.z; // zaxis.y
    result.m22 = eye_dir.z; // zaxis.z

    // Calculate translations - always in 4th column
    result.m30 = -dot(s, eye); // -position.x
    result.m31 = -dot(u, eye); // -position.y
    result.m32 = -dot(eye_dir, eye);
    result.m33 = 1.0f;

    return result;
}

/// @brief Gets a rotation matrix using this matrix and a float3 that describes
/// the axis to rotate around, specifying the angle in degrees to rotate with.
/// @param theta The angle at which to rotate
/// @param axis The vector representing an axis to rotate around
/// @return The rotation matrix
BEE_FORCE_INLINE float4x4 rotate(const float theta, const float3& axis)
{
    float4x4 rotation;

    auto omega = normalize(axis);

    const auto costheta = math::cosf(theta);
    const auto sintheta = math::sinf(theta);
    const auto omega_xsquared = (omega.x * omega.x);
    const auto omega_ysquared = (omega.y * omega.y);
    const auto omega_zsquared = (omega.z * omega.z);
    const auto one_minus_costheta = 1.0f - costheta;

    rotation.m00 = costheta + omega_xsquared * one_minus_costheta;
    rotation.m01 = omega.x * omega.y * one_minus_costheta + omega.z * sintheta;
    rotation.m02 = omega.x * omega.z * one_minus_costheta - omega.y * sintheta;
    rotation.m10 = omega.x * omega.y * one_minus_costheta - omega.z * sintheta;
    rotation.m11 = costheta + omega_ysquared * one_minus_costheta;
    rotation.m12 = omega.y * omega.z * one_minus_costheta + omega.x * sintheta;
    rotation.m20 = omega.x * omega.z * one_minus_costheta + omega.y * sintheta;
    rotation.m21 = omega.y * omega.z * one_minus_costheta - omega.x * sintheta;
    rotation.m22 = costheta + omega_zsquared * one_minus_costheta;
    rotation.m33 = 1.0f;

    return rotation;
}

template <typename PtrType>
float4x4 make_matrix4x4_from_ptr(const PtrType* ptr)
{
    static_assert(sizeof(PtrType) <= sizeof(float), "make_matrix4x4_from_ptr: sizeof(PtrType) "
                                                    "must be <= sizeof(float) otherwise a narrowing "
                                                    "conversion would occur resulting in a loss "
                                                    "of, or incorrect, data");
    float4x4 result;
    memcpy(result.entries, ptr, sizeof(float) * float4x4::num_elements);
    return result;
}


} // namespace bee