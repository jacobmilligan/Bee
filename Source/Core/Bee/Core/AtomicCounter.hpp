/*
 *  AtomicCounter.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/NumericTypes.hpp"

#include <atomic>

namespace bee {


class AtomicCounter : public Noncopyable {
public:
    AtomicCounter() = default;

    explicit AtomicCounter(const i32 value) noexcept
        : value_(value)
    {}

    AtomicCounter(AtomicCounter&& other) noexcept
    {
        const auto loaded_value = other.value_.load(std::memory_order_relaxed);
        value_.store(loaded_value);
    }

    AtomicCounter& operator=(AtomicCounter&& other) noexcept
    {
        const auto loaded_value = other.value_.load(std::memory_order_relaxed);
        value_.store(loaded_value);
        return *this;
    }

    i32 load() const
    {
        return load(std::memory_order_relaxed);
    }

    i32 load(const std::memory_order memory_order) const
    {
        return value_.load(memory_order);
    }

    void store(const i32 value)
    {
        return store(value, std::memory_order_relaxed);
    }

    void store(const i32 value, const std::memory_order memory_order)
    {
        value_.store(value, memory_order);
    }

    i32 count_up(const i32 value, const std::memory_order memory_order)
    {
        auto new_count = load(std::memory_order_acquire) + value;
        if (new_count >= limits::max<i32>() || new_count <= 0) {
            new_count = limits::max<i32>();
        }

        store(new_count, memory_order);
        return new_count;
    }

    i32 count_up(const i32 value)
    {
        return count_up(value, std::memory_order_relaxed);
    }

    i32 count_down(const i32 value, const std::memory_order memory_order)
    {
        auto new_count = load(std::memory_order_acquire) - value;

        if (new_count <= 0) {
            new_count = 0;
        }

        store(new_count, memory_order);
        return new_count;
    }

    i32 count_down(const i32 value)
    {
        return count_down(value, std::memory_order_relaxed);
    }
private:
    std::atomic<i32> value_ { 0 };
};


} // namespace bee