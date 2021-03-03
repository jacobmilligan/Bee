/*
 *  Buffer.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/NumericTypes.hpp"


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