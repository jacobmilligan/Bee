/*
 *  matrix.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Math/quaternion.hpp"

namespace bee {


float4x4::float4x4(const quaternion& quat)
{
    const auto x_sqr = quat.x * quat.x;
    const auto y_sqr = quat.y * quat.y;
    const auto z_sqr = quat.z * quat.z;
    const auto xy = quat.x * quat.y;
    const auto xz = quat.x * quat.z;
    const auto yz = quat.y * quat.z;
    const auto wx = quat.w * quat.x;
    const auto wy = quat.w * quat.y;
    const auto wz = quat.w * quat.z;

    m00 = 1.0f - 2.0f * (y_sqr + z_sqr);
    m01 = 2.0f * (xy - wz);
    m02 = 2.0f * (xz + wy);
    m03 = 0.0f;
    m10 = 2.0f * (xy + wz);
    m11 = 1.0f - 2.0f * (x_sqr + z_sqr);
    m12 = 2.0f * (yz - wx);
    m13 = 0.0f;
    m20 = 2.0f * (xz - wy);
    m21 = 2.0f * (yz + wx);
    m22 = 1.0f - 2.0f * (x_sqr + y_sqr);
    m23 = 0.0f;
    m30 = 0.0f;
    m31 = 0.0f;
    m32 = 0.0f;
    m33 = 0.0f;
}

float4x4 ortho(const float left, const float right, const float bottom,
                                const float top, const float near, const float far)
{
    float4x4 result;
    const float size_x = right - left;
    const float size_y = top - bottom;
    const float zoom = far - near;

    result.m00 = 2.0f / size_x;
    result.m11 = 2.0f / size_y;
    result.m22 = -2.0f / zoom;
    result.m30 = -(right + left) / size_x;
    result.m31 = -(top + bottom) / size_y;
    result.m32 = -(far + near) / zoom;
    result.m33 = 1.0f;

    return result;
}


} // namespace bee