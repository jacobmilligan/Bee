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
    static u64 value = ticks_per_second() / 1000000;
    return value;
}

u64 ticks_per_millisecond() noexcept
{
    static u64 value = ticks_per_second() / 1000;
    return value;
}

u64 ticks_per_minute() noexcept
{
    static u64 value = ticks_per_second() / 60;
    return value;
}

u64 ticks_per_hour() noexcept
{
    static u64 value = (ticks_per_second() / 60) / 60;
    return value;
}

u64 microseconds(const u64 us)
{
    return ticks_per_microsecond() * us;
}

u64 milliseconds(const u64 ms)
{
    return ticks_per_millisecond() * ms;
}

u64 seconds(const u64 s)
{
    return ticks_per_second() * s;
}

u64 minutes(const u64 m)
{
    return ticks_per_minute() * m;
}

u64 hours(const u64 h)
{
    return ticks_per_hour() * h;
}


static const double microseconds_per_tick = 1.0 / static_cast<double>(ticks_per_microsecond());

static const double milliseconds_per_tick = 1.0 / static_cast<double>(ticks_per_millisecond());

static const double seconds_per_tick = 1.0 / static_cast<double>(ticks_per_second());

static const double minutes_per_tick = 1.0 / static_cast<double>(ticks_per_minute());

static const double hours_per_tick = 1.0 / static_cast<double>(ticks_per_hour());


double total_microseconds(const u64 ticks)
{
    return static_cast<double>(ticks) * microseconds_per_tick;
}

double total_milliseconds(const u64 ticks)
{
    return static_cast<double>(ticks) * milliseconds_per_tick;
}

double total_seconds(const u64 ticks)
{
    return static_cast<double>(ticks) * seconds_per_tick;
}

double total_minutes(const u64 ticks)
{
    return static_cast<double>(ticks) * minutes_per_tick;
}

double total_hours(const u64 ticks)
{
    return static_cast<double>(ticks) * hours_per_tick;
}


} // namespace time

TimePoint::TimePoint(const double hours, const double minutes, const double seconds)
{
    const auto time_total = hours * 3600 + minutes * 60 + seconds;
    ticks_ = static_cast<u64>(time_total * time::ticks_per_second());
}

u64 TimePoint::microseconds() const
{
    return static_cast<u64>(ticks_ * time::microseconds_per_tick);
}

u64 TimePoint::milliseconds() const
{
    return static_cast<u64>(ticks_ * time::milliseconds_per_tick);
}

u64 TimePoint::seconds() const
{
    return static_cast<u64>(ticks_ * time::seconds_per_tick);
}

u64 TimePoint::minutes() const
{
    return static_cast<u64>(ticks_ * time::minutes_per_tick);
}

u64 TimePoint::hours() const
{
    return static_cast<u64>(ticks_ * time::hours_per_tick);
}

double TimePoint::total_microseconds() const
{
    return time::total_microseconds(ticks_);
}

double TimePoint::total_milliseconds() const
{
    return time::total_milliseconds(ticks_);
}

double TimePoint::total_seconds() const
{
    return time::total_seconds(ticks_);
}

double TimePoint::total_minutes() const
{
    return time::total_minutes(ticks_);
}

double TimePoint::total_hours() const
{
    return time::total_hours(ticks_);
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

bool TimePoint::operator==(const TimePoint& other) const
{
    return ticks_ == other.ticks_;
}

bool TimePoint::operator!=(const TimePoint& other) const
{
    return ticks_ != other.ticks_;
}

bool TimePoint::operator>(const TimePoint& other) const
{
    return ticks_ > other.ticks_;
}

bool TimePoint::operator<(const TimePoint& other) const
{
    return ticks_ < other.ticks_;
}

bool TimePoint::operator>=(const TimePoint& other) const
{
    return ticks_ >= other.ticks_;
}

bool TimePoint::operator<=(const TimePoint& other) const
{
    return ticks_ <= other.ticks_;
}



} // namespace bee
