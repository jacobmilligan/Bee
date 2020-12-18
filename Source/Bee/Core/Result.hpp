/*
 *  Result.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Error.hpp"

namespace bee {


template <typename T, typename E>
class Result
{
public:
    Result(Result&& other) noexcept
    {
        move_construct(other);
    }

    Result(const Result& other) noexcept
    {
        copy_construct(other);
    }

    Result(const E& error) noexcept
        : state_(State::err)
    {
        new (&get_err()) E(error);
    }

    Result(E&& error) noexcept
        : state_(State::err)
    {
        new (&get_err()) E(BEE_MOVE(error));
    }

    Result(const T& value) noexcept
        : state_(State::ok)
    {
        new (&get_value()) T(value);
    }

    Result(T&& value) noexcept
        : state_(State::ok)
    {
        new (&get_value()) T(BEE_MOVE(value));
    }

    ~Result()
    {
        destroy();
    }

    inline Result<T, E>& operator=(const Result<T, E>& other)
    {
        copy_construct(other);
        return *this;
    }

    inline Result<T, E>& operator=(Result<T, E>&& other)
    {
        move_construct(other);
        return *this;
    }

    inline bool is_ok() const noexcept
    {
        return state_ == State::ok;
    }

    inline bool is_error() const noexcept
    {
        return state_ == State::err;
    }

    inline T& unwrap()
    {
        BEE_ASSERT_F(is_ok(), "Failed to unwrap Result<T, E> with Error status");
        return get_value();
    }

    inline const E& unwrap_error() const
    {
        BEE_ASSERT_F(is_error(), "Failed to unwrap Result<T, E> with Ok status");
        return get_err();
    }

    inline T& expect(const char* format, ...) BEE_PRINTFLIKE(2, 3)
    {
        BEE_ASSERT_F(is_ok(), format, __VA_ARGS__); // NOLINT
        return get_value();
    }

    inline const T& expect_error(const char* format, ...) const BEE_PRINTFLIKE(2, 3)
    {
        BEE_ASSERT_F(is_error(), format, __VA_ARGS__); // NOLINT
        return get_value();
    }

    inline operator bool() const
    {
        return is_ok();
    }

private:
    static constexpr size_t storage_size_ = sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E);

    enum class State : u8
    {
        uninitialized,
        ok,
        err
    };

    State   state_ { State::uninitialized };
    u8      storage_[storage_size_];

    T& get_value()
    {
        return *reinterpret_cast<T*>(storage_);
    }

    const T& get_value() const
    {
        return *reinterpret_cast<const T*>(storage_);
    }

    E& get_err()
    {
        return *reinterpret_cast<E*>(storage_);
    }

    const E& get_err() const
    {
        return *reinterpret_cast<const E*>(storage_);
    }

    void destroy()
    {
        if (state_ == State::uninitialized)
        {
            return;
        }

        if (is_ok())
        {
            destruct(&get_value());
        }
        else
        {
            destruct(&get_err());
        }
    }

    void move_construct(Result<T, E>& other) noexcept
    {
        destroy();

        state_ = other.state_;
        if (is_ok())
        {
            get_value() = BEE_MOVE(other.get_value());
        }
        else
        {
            get_err() = BEE_MOVE(other.get_err());
        }
    }

    void copy_construct(const Result<T, E>& other)
    {
        destroy();

        state_ = other.state_;
        if (is_ok())
        {
            new(&get_value()) T(other.get_value());
        }
        else
        {
            new(&get_err()) E(other.get_err());
        }
    }
};


} // namespace bee