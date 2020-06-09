/*
 *  Container.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/declval.hpp"

#include <type_traits>
#include <string.h> // for memcpy


namespace bee {


/*
* Container interface - can be either dynamic or fixed in size
*/

enum class BEE_REFLECT(serializable) ContainerMode
{
    fixed_capacity,
    dynamic_capacity
};

template <ContainerMode Mode>
struct container_mode_constant
{
    static constexpr ContainerMode mode = Mode;
};

using fixed_container_mode_t = container_mode_constant<ContainerMode::fixed_capacity>;

using dynamic_container_mode_t = container_mode_constant<ContainerMode::dynamic_capacity>;

/*
 * # copy
 */
template <typename T>
inline void memcpy_or_assign(T* dst, const T* src, const i32 count, const std::true_type& /* true_type */)
{
    memcpy(dst, src, sizeof(T) * count);
}

template <typename T>
inline void memcpy_or_assign(T* dst, const T* src, const i32 count, const std::false_type& /* false_type */)
{
    for (int index = 0; index < count; ++index)
    {
        dst[index] = src[index];
    }
}

template <typename T>
inline void memcpy_or_construct(T* dst, const T* src, const i32 count, const std::true_type& /* true_type */)
{
    memcpy(dst, src, sizeof(T) * count);
}

template <typename T>
inline void memcpy_or_construct(T* dst, const T* src, const i32 count, const std::false_type& /* false_type */)
{
    for (int index = 0; index < count; ++index)
    {
        new (dst + index) T(src[index]);
    }
}


/**
 * Copies the `src` range into `dst`. If `T` is trivially-copyable memcpy is used, otherwise the copy-assign operator
 * for`T` is used.
 */
template <typename T>
inline void copy(T* dst, const T* src, const i32 count)
{
    memcpy_or_assign(dst, src, count, std::is_trivially_copyable<T>{});
}

/**
 * Copies the `src` range into `dst` which MUST be uninitialized memory. Similar to `bee::copy` but uses placement-new
 * and the copy constructor of `T` instead of copy-assign.
 */
template <typename T>
inline void copy_uninitialized(T* dst, const T* src, const i32 count)
{
    memcpy_or_construct(dst, src, count, std::is_trivially_copyable<T>{});
}


/*
 * # begin/end
 *
 * Gets an iterator to the beginning or end of a container that defines begin/end member functions or c-style array
 */

template <typename ContainerType>
inline constexpr auto begin(ContainerType& container) -> decltype(container.begin())
{
    return container.begin();
}

template <typename ContainerType>
inline constexpr auto begin(const ContainerType& container) -> decltype(container.begin())
{
    return container.begin();
}

template <typename ContainerType>
inline constexpr auto end(ContainerType& container) -> decltype(container.end())
{
    return container.end();
}

template <typename ContainerType>
inline constexpr auto end(const ContainerType& container) -> decltype(container.end())
{
    return container.end();
}

template <typename T, i32 Size>
inline constexpr T* begin(T(&array)[Size])
{
    return array;
}

template <typename T, i32 Size>
inline constexpr const T* begin(const T(&array)[Size])
{
    return array;
}

template <typename T, i32 Size>
inline constexpr T* end(T(&array)[Size])
{
    return array + Size;
}

template <typename T, i32 Size>
inline constexpr const T* end(const T(&array)[Size])
{
    return array + Size;
}


/*
 * # `enumerate`
 *
 * Range-based for loop adapter for iterating the item and the index in a contiguous collection - can be used with any
 * container that defines a begin()/end() pair
 */
template <typename ElementType>
struct EnumeratorRef
{
    i32             index { 0 };
    ElementType&    value;
};

template <typename T>
EnumeratorRef<T> make_enumerator_ref(const i32 index, T& value)
{
    return EnumeratorRef<T> { index, value };
}

template <typename IterableType>
class Enumerator
{
public:
    using iterator_t = decltype(::bee::begin(declval<IterableType>()));

    explicit Enumerator(iterator_t begin)
        : iterator_(begin)
    {}

    Enumerator(const Enumerator& other)
        : iterator_(other.iterator_),
          index_(other.index_)
    {}

    Enumerator& operator=(const Enumerator& other)
    {
        iterator_ = other.iterator_;
        index_ = other.index_;
        return *this;
    }

    bool operator==(const Enumerator& other) const
    {
        return iterator_ == other.iterator_;
    }

    bool operator!=(const Enumerator& other) const
    {
        return !(*this == other);
    }

    auto operator*() const
    {
        return make_enumerator_ref(index_, *iterator_);
    }

    Enumerator<IterableType>& operator++()
    {
        ++iterator_;
        ++index_;
        return *this;
    }

private:
    iterator_t  iterator_;
    i32         index_ { 0 };
};

template <typename IterableType>
struct EnumerateAdapter
{
    using iterable_t = IterableType;

    IterableType&  iterable;

    explicit EnumerateAdapter(IterableType& new_iterable)
        : iterable(new_iterable)
    {}

    inline Enumerator<IterableType> begin() const
    {
        return Enumerator<IterableType>(::bee::begin(iterable));
    }

    inline Enumerator<IterableType> end() const
    {
        return Enumerator<IterableType>(::bee::end(iterable));
    }
};

template <typename IterableType>
inline constexpr EnumerateAdapter<const IterableType> enumerate(const IterableType& iterable)
{
    return EnumerateAdapter<const IterableType>(iterable);
}

template <typename IterableType>
inline constexpr EnumerateAdapter<IterableType> enumerate(IterableType& iterable)
{
    return EnumerateAdapter<IterableType>(iterable);
}


/*
 * # `index_of`
 *
 * Gets the index of a value in a c-array or container
 */
template <typename T, typename PredicateType>
inline constexpr i32 find_index_if(const T* begin, const T* end, PredicateType&& pred)
{
    auto* value = begin;
    while (value != end)
    {
        if (pred(*value))
        {
            return static_cast<i32>(value - begin);
        }
        ++value;
    }
    return -1;
}

template <typename ContainerType, typename PredicateType>
inline constexpr i32 find_index_if(const ContainerType& container, PredicateType&& pred)
{
    return find_index_if(::bee::begin(container), ::bee::end(container), pred);
}

template <typename T>
inline constexpr i32 find_index(const T* begin, const T* end, const T& to_find)
{
    return find_index_if(begin, end, [&](const T& value) { return value == to_find; });
}

template <typename ContainerType, typename T>
inline constexpr i32 find_index(const ContainerType& container, const T& to_find)
{
    return find_index(::bee::begin(container), ::bee::end(container), to_find);
}



} // namespace bee