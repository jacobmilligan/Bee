/*
 *  Math.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Math/quaternion.hpp"

#include <cmath>


namespace bee {

const quaternion& quaternion::identity()
{
    static auto identity_quat = quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    return identity_quat;
}

namespace math {


double sqrt(const double value)
{
    return std::sqrt(value);
}

float sqrtf(const float value)
{
    return std::sqrtf(value);
}

double pow(const double base, const double exponent)
{
    return std::pow(base, exponent);
}

float powf(const float base, const float exponent)
{
    return std::powf(base, exponent);
}

double floor(const double value)
{
    return std::floor(value);
}

float floorf(const float value)
{
    return std::floorf(value);
}

double ceil(double value)
{
    return std::ceil(value);
}

float ceilf(float value)
{
    return std::ceilf(value);
}

/*
 * Trigonometric functions
 */

float acosf(const float value)
{
    return std::acosf(value);
}

double acos(const double value)
{
    return std::acos(value);
}

float asinf(const float value)
{
    return std::asinf(value);
}

double asin(const double value)
{
    return std::asin(value);
}

float atanf(const float value)
{
    return std::atanf(value);
}

double atan(const double value)
{
    return std::atan(value);
}

float atan2f(const float y, const float x)
{
    return std::atan2f(y, x);
}

double atan2(const double y, const double x)
{
    return std::atan2(y, x);
}

float cosf(const float value)
{
    return std::cosf(value);
}

double cos(const double value)
{
    return std::cos(value);
}

float sinf(const float value)
{
    return std::sinf(value);
}

double sin(const double value)
{
    return std::sin(value);
}

float tanf(const float value)
{
    return std::tanf(value);
}

double tan(const double value)
{
    return std::tan(value);
}

float acoshf(const float value)
{
    return std::acoshf(value);
}

double acosh(const double value)
{
    return std::acosh(value);
}

float asinhf(const float value)
{
    return std::asinhf(value);
}

double asinh(const double value)
{
    return std::asinh(value);
}

float atanhf(const float value)
{
    return std::atanhf(value);
}

double atanh(const double value)
{
    return std::atanh(value);
}

float coshf(const float value)
{
    return std::coshf(value);
}

double cosh(const double value)
{
    return std::cosh(value);
}

float sinhf(const float value)
{
    return std::sinhf(value);
}

double sinh(const double value)
{
    return std::sinh(value);
}

float tanhf(const float value)
{
    return std::tanhf(value);
}

double tanh(const double value)
{
    return std::tanh(value);
}

double abs(const double value)
{
    return std::abs(value);
}

float fabs(const float value)
{
    return std::fabsf(value);
}

i32 iabs(const i32 value)
{
    return value >= 0 ? value : -value;
}


}
} // namespace bee
