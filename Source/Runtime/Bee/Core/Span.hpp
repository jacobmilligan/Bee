/*
 *  Span.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {


template <typename T>
class Span {
public:
    using value_t       = T;
    using reference_t   = T&;
    using pointer_t     = T*;

    constexpr Span() noexcept = default;

    constexpr Span(T* data, i32 size) noexcept;

    constexpr Span(T* first, T* last) noexcept;

    template <i32 Size>
    constexpr explicit Span(T(&static_array)[Size]) noexcept; // NOLINT(google-explicit-constructor)

    constexpr Span(const Span<T>& other) noexcept = default;

    constexpr Span<T>& operator=(const Span<T>& other) noexcept = default;

    constexpr T* begin() const;

    constexpr T* end() const;

    T& operator[](i32 index) const;

    constexpr T* data() const noexcept;

    constexpr i32 size() const;

    constexpr i32 byte_size() const;

    constexpr size_t memory_size() const;

    constexpr bool empty() const;

    Span<T> subspan(i32 offset, i32 count) const;

    Span<u8> to_bytes() const;
private:
    T*          data_ { nullptr };
    i32         size_ { 0 };
};

template <typename T>
constexpr Span<T>::Span(T* data, const i32 size) noexcept
    : data_(data),
      size_(size)
{}

template <typename T>
constexpr Span<T>::Span(T* first, T* last) noexcept
    : data_(first),
      size_(sign_cast<i32>(last - first))
{
    static_assert(last > first, "Last must be at a higher address than first");
}

template <typename T>
template <i32 Size>
constexpr Span<T>::Span(T(&static_array)[Size]) noexcept
    : data_(static_array),
      size_(Size)
{}

template <typename T>
constexpr T* Span<T>::begin() const
{
    return data_;
}

template <typename T>
constexpr T* Span<T>::end() const
{
    return data_ + size_;
}

template <typename T>
T& Span<T>::operator[](const i32 index) const
{
    BEE_ASSERT(index < size_);
    return data_[index];
}

template <typename T>
constexpr T* Span<T>::data() const noexcept
{
    return data_;
}

template <typename T>
constexpr i32 Span<T>::size() const
{
    return size_;
}

template <typename T>
constexpr i32 Span<T>::byte_size() const
{
    return sizeof(T) * size_;
}

template <typename T>
constexpr size_t Span<T>::memory_size() const
{
    return sizeof(T) * sign_cast<size_t>(size_);
}

template <typename T>
constexpr bool Span<T>::empty() const
{
    return data_ == nullptr || size_ <= 0;
}

template <typename T>
Span<T> Span<T>::subspan(const i32 offset, const i32 count) const
{
    BEE_ASSERT(offset < size_);
    BEE_ASSERT(offset + count <= size_);
    return Span<T>(&data_[offset], count);
}

template <typename T>
Span<u8> Span<T>::to_bytes() const
{
    return Span<u8>(reinterpret_cast<u8*>(data_), byte_size());
}

/*
 * make_span: non-const
 */

template <typename T>
constexpr Span<T> make_span(T* data, const i32 size) noexcept
{
    return Span<T>(data, size);
}

template <typename T>
constexpr Span<T> make_span(T* first, T* last) noexcept
{
    return Span<T>(first, last);
}

template <typename T, i32 Size>
constexpr Span<T> make_span(T(&static_array)[Size]) noexcept
{
    return Span<T>(static_array);
}


/*
 * make_span: const qualified implicit overloads
 */

template <typename T>
constexpr Span<const T> make_span(const T* data, const i32 size) noexcept
{
    return Span<const T>(data, size);
}

template <typename T>
constexpr Span<const T> make_span(const T* first, const T* last) noexcept
{
    return Span<const T>(first, last);
}

template <typename T, i32 Size>
constexpr Span<const T> make_span(const T(&static_array)[Size]) noexcept
{
    return Span<const T>(static_array);
}

/*
 * make_const_span: explicit const-qualified make_span overloads
 */

template <typename T>
constexpr Span<const T> make_const_span(const T* data, const i32 size) noexcept
{
    return make_span(data, size);
}

template <typename T>
constexpr Span<const T> make_const_span(const T* first, const T* last) noexcept
{
    return make_span(first, last);
}

template <typename T, i32 Size>
constexpr Span<const T> make_const_span(const T(&static_array)[Size]) noexcept
{
    return make_span(static_array);
}


} // namespace bee