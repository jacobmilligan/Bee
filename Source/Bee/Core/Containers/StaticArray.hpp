/*
 *  Buffer.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Span.hpp"


namespace bee {


template <typename T, i32 Capacity, typename SizeType = i32>
struct BEE_REFLECT(serializable) StaticArray
{
    static constexpr SizeType capacity = Capacity;

    T           data[Capacity];
    SizeType    size { 0 };
    BEE_PAD(8 - sizeof(SizeType));

    T& operator[](const SizeType index)
    {
        return data[index];
    }

    const T& operator[](const SizeType index) const
    {
        return data[index];
    }

    const T* begin() const
    {
        return data;
    }

    T* begin()
    {
        return data;
    }

    const T* end() const
    {
        return data + size;
    }

    T* end()
    {
        return data + size;
    }

    inline bool empty() const
    {
        return size <= 0;
    }

    inline const Span<const T> const_span() const
    {
        return make_const_span(data, size);
    }

    inline Span<T> span()
    {
        return make_span(data, size);
    }

    inline T& back()
    {
        return data[size - 1];
    }

    inline const T& back() const
    {
        return data[size - 1];
    }

    void push_back(const T& value)
    {
        BEE_ASSERT(size <= capacity);
        new (&data[size]) T(value);
        ++size;
    }

    void push_back(T&& value)
    {
        BEE_ASSERT(size <= capacity);
        ::bee::move_range(data + size, &value, 1);
        ++size;
    }

    template <class... Args>
    void emplace_back(Args&&... args)
    {
        BEE_ASSERT(size <= capacity);
        new (&data[size]) T(BEE_FORWARD(args)...);
        ++size;
    }
};

template <typename T, i32 Capacity, typename SizeType>
bool bitwise_equal(const StaticArray<T, Capacity, SizeType>& lhs, const StaticArray<T, Capacity, SizeType>& rhs)
{
    return lhs.size == rhs.size && memcmp(lhs.data, rhs.data, sizeof(T) * lhs.size) == 0;
}


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.Core/ReflectedTemplates/StaticArray.generated.inl"
#endif // BEE_ENABLE_REFLECTION