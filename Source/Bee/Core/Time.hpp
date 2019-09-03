//
//  Time.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 4/08/2018
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {
namespace time {

/// Queries the systems high-resolution timer for the current time since startup
BEE_API u64 now() noexcept;

/// Returns the number of ticks per second of the current system 
BEE_API u64 ticks_per_second() noexcept;


} // namespace time


enum class TimeInterval
{
    ticks,
    microseconds,
    milliseconds,
    seconds,
    minutes,
    hours
};

class BEE_API TimePoint {
public:
    /// Represents the amount of ticks that occur for every microsecond
    static const u64 ticks_per_microsecond;
    /// Represents the amount of ticks that occur for every millisecond
    static const u64 ticks_per_millisecond;
    /// Represents the amount of ticks that occur for every second
    static const u64 ticks_per_second;
    /// Represents the amount of ticks that occur for every minute
    static const u64 ticks_per_minute;
    /// Represents the amount of ticks that occur for every hour
    static const u64 ticks_per_hour;

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

    bool operator==(const TimePoint& other);

    bool operator!=(const TimePoint& other);

    bool operator>(const TimePoint& other);

    bool operator<(const TimePoint& other);

    bool operator>=(const TimePoint& other);

    bool operator<=(const TimePoint& other);
private:
    /// The fraction of a microsecond that each system tick is worth
    static const double microseconds_per_tick;
    /// The fraction of a millisecond that each system tick is worth
    static const double milliseconds_per_tick;
    /// The fraction of a second that each system tick is worth
    static const double seconds_per_tick;
    /// The fraction of a minute that each system tick is worth
    static const double minutes_per_tick;
    /// The fraction of a hours that each system tick is worth
    static const double hours_per_tick;

    /// timepoint value in raw system ticks
    u64 ticks_ { 0 };
};

template<TimeInterval Interval>
TimePoint make_time_point(u64 ticks);

template<>
inline TimePoint make_time_point<TimeInterval::ticks>(const u64 ticks)
{
    return TimePoint(ticks);
}

template<>
inline TimePoint make_time_point<TimeInterval::microseconds>(const u64 ticks)
{
    return TimePoint(static_cast<u64>(TimePoint::ticks_per_microsecond * ticks));
}

template<>
inline TimePoint make_time_point<TimeInterval::milliseconds>(const u64 ticks)
{
    return TimePoint(static_cast<u64>(TimePoint::ticks_per_millisecond * ticks));
}

template<>
inline TimePoint make_time_point<TimeInterval::seconds>(const u64 ticks)
{
    return TimePoint(static_cast<u64>(TimePoint::ticks_per_second * ticks));
}

template<>
inline TimePoint make_time_point<TimeInterval::minutes>(const u64 ticks)
{
    return TimePoint(static_cast<u64>(TimePoint::ticks_per_minute * ticks));
}

template<>
inline TimePoint make_time_point<TimeInterval::hours>(const u64 ticks)
{
    return TimePoint(static_cast<u64>(TimePoint::ticks_per_hour * ticks));
}


} // namespace bee
