/*
 *  Container.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/declval.hpp"


namespace bee {


/*
* Container interface - can be either dynamic or fixed in size
*/

enum class ContainerMode
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
 * # add
 */

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
inline constexpr T* begin(const T(&array)[Size])
{
    return array;
}

template <typename T, i32 Size>
inline constexpr T* end(T(&array)[Size])
{
    return array + Size;
}

template <typename T, i32 Size>
inline constexpr T* end(const T(&array)[Size])
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
    using iterator_t = decltype(begin(declval<IterableType>()));

    Enumerator(iterator_t begin)
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
template <typename ContainerType, typename PredicateType>
inline constexpr i32 container_index_of(const ContainerType& container, PredicateType&& pred)
{
    int index = 0;
    for (const auto& value : container)
    {
        if (pred(value))
        {
            return index;
        }
        ++index;
    }
    return -1;
}


} // namespace bee