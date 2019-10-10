/*
 *  half.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {

static constexpr u16 half_max = 0xFFE0;

class BEE_CORE_API half {
public:
    half()
        : val_(0)
    {}

    explicit half(const u16 value)
        : val_(value)
    {}

    explicit half(const float value)
        : val_(half_from_float(value))
    {}

    half(const half& other) = default;

    half& operator=(const half& other) = default;
    half& operator=(const float& other)
    {
        val_ = half_from_float(other);
        return *this;
    }

    inline float to_float() const
    {
        return half_to_float(val_);
    }

    inline u16 value() const
    {
        return val_;
    }

    explicit operator float() const
    {
        return to_float();
    }

    half operator+(const half& other)
    {
        return half(half_add(val_, other.val_));
    }

    half& operator+=(const half& other)
    {
        val_ = half_add(val_, other.val_);
        return *this;
    }

    half operator+(const float other)
    {
        return half(half_add(val_, half_from_float(other)));
    }

    half& operator+=(const float other)
    {
        val_ = half_add(val_, half_from_float(other));
        return *this;
    }

    bool operator==(const half& other)
    {
        return val_ == other.val_;
    }

    bool operator!=(const half& other)
    {
        return val_ != other.val_;
    }
private:
    u16 val_ { 0 };

    float half_to_float(u16 h) const;
    u16 half_from_float(float f) const;
    u16 half_add(u16 x, u16 y) const;
};


} // namespace bee