/*
 *  Time.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Time.hpp"


namespace bee {
namespace time {


u64 ticks_per_microsecond() noexcept
{
    static u64 value = time::ticks_per_second() / 1000000;
    return value;
}

u64 ticks_per_millisecond() noexcept
{
    static u64 value = time::ticks_per_second() / 1000;
    return value;
}

u64 ticks_per_minute() noexcept
{
    static u64 value = time::ticks_per_second() / 60;
    return value;
}

u64 ticks_per_hour() noexcept
{
    static u64 value = (time::ticks_per_second() / 60) / 60;
    return value;
}


} // namespace time

const double TimePoint::microseconds_per_tick = 1.0 / static_cast<double>(time::ticks_per_microsecond());

const double TimePoint::milliseconds_per_tick = 1.0 / static_cast<double>(time::ticks_per_millisecond());

const double TimePoint::seconds_per_tick = 1.0 / static_cast<double>(time::ticks_per_second());

const double TimePoint::minutes_per_tick = 1.0 / static_cast<double>(time::ticks_per_minute());

const double TimePoint::hours_per_tick = 1.0 / static_cast<double>(time::ticks_per_hour());

TimePoint::TimePoint(const double hours, const double minutes, const double seconds)
{
    const auto time_total = hours * 3600 + minutes * 60 + seconds;
    ticks_ = static_cast<u64>(time_total * time::ticks_per_second());
}

u64 TimePoint::microseconds() const
{
    return static_cast<u64>(ticks_ * microseconds_per_tick);
}

u64 TimePoint::milliseconds() const
{
    return static_cast<u64>(ticks_ * milliseconds_per_tick);
}

u64 TimePoint::seconds() const
{
    return static_cast<u64>(ticks_ * seconds_per_tick);
}

u64 TimePoint::minutes() const
{
    return static_cast<u64>(ticks_ * minutes_per_tick);
}

u64 TimePoint::hours() const
{
    return static_cast<u64>(ticks_ * hours_per_tick);
}

double TimePoint::total_microseconds() const
{
    return ticks_ * microseconds_per_tick;
}

double TimePoint::total_milliseconds() const
{
    return ticks_ * milliseconds_per_tick;
}

double TimePoint::total_seconds() const
{
    return ticks_ * seconds_per_tick;
}

double TimePoint::total_minutes() const
{
    return ticks_ * minutes_per_tick;
}

double TimePoint::total_hours() const
{
    return ticks_ * hours_per_tick;
}

TimePoint TimePoint::operator-(const TimePoint& other) const
{
    return TimePoint(ticks_ - other.ticks_);
}

TimePoint TimePoint::operator+(const TimePoint& other) const
{
    return TimePoint(ticks_ + other.ticks_);
}

TimePoint TimePoint::operator*(const TimePoint& other) const
{
    return TimePoint(ticks_ * other.ticks_);
}

TimePoint TimePoint::operator/(const TimePoint& other) const
{
    return TimePoint(ticks_ / other.ticks_);
}

TimePoint& TimePoint::operator-=(const TimePoint& other)
{
    ticks_ -= other.ticks_;
    return *this;
}

TimePoint& TimePoint::operator+=(const TimePoint& other)
{
    ticks_ += other.ticks_;
    return *this;
}

TimePoint& TimePoint::operator*=(const TimePoint& other)
{
    ticks_ *= other.ticks_;
    return *this;
}

TimePoint& TimePoint::operator/=(const TimePoint& other)
{
    ticks_ /= other.ticks_;
    return *this;
}

bool TimePoint::operator==(const TimePoint& other)
{
    return ticks_ == other.ticks_;
}

bool TimePoint::operator!=(const TimePoint& other)
{
    return ticks_ != other.ticks_;
}

bool TimePoint::operator>(const TimePoint& other)
{
    return ticks_ > other.ticks_;
}

bool TimePoint::operator<(const TimePoint& other)
{
    return ticks_ < other.ticks_;
}

bool TimePoint::operator>=(const TimePoint& other)
{
    return ticks_ >= other.ticks_;
}

bool TimePoint::operator<=(const TimePoint& other)
{
    return ticks_ <= other.ticks_;
}



} // namespace bee
