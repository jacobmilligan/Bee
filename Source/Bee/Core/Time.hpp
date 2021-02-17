/*
 *  Time.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"


namespace bee {
namespace io {


class StringStream;


}


namespace time {

/// Queries the systems high-resolution timer for the current time since startup
BEE_CORE_API u64 now() noexcept;

/// Returns the number of CPU ticks per second of the current system - this is a platform specific call
BEE_CORE_API u64 ticks_per_second() noexcept;

/// Represents the amount of ticks that occur for every microsecond
BEE_CORE_API u64 ticks_per_microsecond() noexcept;
/// Represents the amount of ticks that occur for every millisecond
BEE_CORE_API u64 ticks_per_millisecond() noexcept;
/// Represents the amount of ticks that occur for every minute
BEE_CORE_API u64 ticks_per_minute() noexcept;
/// Represents the amount of ticks that occur for every hour
BEE_CORE_API u64 ticks_per_hour() noexcept;

BEE_CORE_API u64 microseconds(const u64 us);

BEE_CORE_API u64 milliseconds(const u64 ms);

BEE_CORE_API u64 seconds(const u64 s);

BEE_CORE_API u64 minutes(const u64 m);

BEE_CORE_API u64 hours(const u64 h);

BEE_CORE_API double total_microseconds(const u64 ticks);

BEE_CORE_API double total_milliseconds(const u64 ticks);

BEE_CORE_API double total_seconds(const u64 ticks);

BEE_CORE_API double total_minutes(const u64 ticks);

BEE_CORE_API double total_hours(const u64 ticks);

} // namespace time


class BEE_CORE_API TimePoint {
public:
    TimePoint() = default;

    explicit TimePoint(u64 ticks)
        : ticks_(ticks)
    {}

    TimePoint(const double hours, const double minutes, const double seconds);

    BEE_FORCE_INLINE void reset(const u64 ticks)
    {
        ticks_ = ticks;
    }

    BEE_FORCE_INLINE u64 ticks() const
    {
        return ticks_;
    }

    u64 microseconds() const;

    u64 milliseconds() const;

    u64 seconds() const;

    u64 minutes() const;

    u64 hours() const;

    double total_microseconds() const;

    double total_milliseconds() const;

    double total_seconds() const;

    double total_minutes() const;

    double total_hours() const;

    TimePoint operator-(const TimePoint& other) const;

    TimePoint operator+(const TimePoint& other) const;

    TimePoint operator*(const TimePoint& other) const;

    TimePoint operator/(const TimePoint& other) const;

    TimePoint& operator-=(const TimePoint& other);

    TimePoint& operator+=(const TimePoint& other);

    TimePoint& operator*=(const TimePoint& other);

    TimePoint& operator/=(const TimePoint& other);

    bool operator==(const TimePoint& other) const;

    bool operator!=(const TimePoint& other) const;

    bool operator>(const TimePoint& other) const;

    bool operator<(const TimePoint& other) const;

    bool operator>=(const TimePoint& other) const;

    bool operator<=(const TimePoint& other) const;
private:
    /// timepoint value in raw system ticks
    u64 ticks_ { 0 };
};


} // namespace bee
