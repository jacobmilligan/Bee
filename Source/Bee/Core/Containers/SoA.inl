/*
 *  SoA.inl
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


namespace bee {


/**
 * Calls the destructors for elements in a single SoA column
 */
template <typename... Types>
struct column_destructor;

template <typename T>
struct column_destructor<T>
{
    static void destroy(u8* array_ptr, const i32 element_index, const i32 soa_capacity) noexcept
    {
        auto array = reinterpret_cast<T*>(array_ptr);
        array[element_index].~T();
    }
};

template <typename FirstType, typename... Types>
struct column_destructor<FirstType, Types...>
{
    static void destroy(u8* array_ptr, const i32 element_index, const i32 soa_capacity) noexcept
    {
        auto array = reinterpret_cast<FirstType*>(array_ptr);
        array[element_index].~FirstType();
        const auto array_end = sizeof(FirstType) * (soa_capacity - element_index);
        column_destructor<Types...>::destroy(array_ptr + array_end, element_index, soa_capacity);
    }
};

/**
 * Calls the constructors for elements in a single SoA column
 */
template <typename... Types>
struct column_constructor;

template <typename T>
struct column_constructor<T>
{
    static void construct(u8** array_ptrs, const i32 array_index, const i32 element_index, const T& value) noexcept
    {
        auto array = reinterpret_cast<T*>(array_ptrs[array_index]);
        new (array + element_index) T(value);
    }
};

template <typename FirstType, typename... RemainingTypes>
struct column_constructor<FirstType, RemainingTypes...>
{
    static void construct(
        u8** array_ptrs,
        const i32 array_index,
        const i32 element_index,
        const FirstType& value,
        const RemainingTypes&... remaining
    ) noexcept
    {
        auto array = reinterpret_cast<FirstType*>(array_ptrs[array_index]);
        new (array + element_index) FirstType(value);
        column_constructor<RemainingTypes...>::construct(array_ptrs, array_index + 1, element_index, remaining...);
    }
};


/**
 * Assigns into an array of pointers to arrays using a set of types, soa capacity and their indices
 */
template <typename... Types>
struct get_array_pointers;

template <typename T>
struct get_array_pointers<T>
{
    static constexpr void assign(u8* data, u8** array_ptrs, const i32 type_index, const i32 soa_capacity) noexcept
    {
        array_ptrs[type_index] = data;
    }
};

template <typename FirstType, typename... RemainingTypes>
struct get_array_pointers<FirstType, RemainingTypes...>
{
    static constexpr void assign(u8* data, u8** array_ptrs, const i32 type_index, const i32 soa_capacity) noexcept
    {
        const auto next_type_offset = sizeof(FirstType) * soa_capacity;
        array_ptrs[type_index] = data;
        get_array_pointers<RemainingTypes...>::assign(data + next_type_offset, array_ptrs, type_index + 1, soa_capacity);
    }
};



/*
 ****************************
 *
 * # SoA - implementation
 *
 ****************************
 */
template <typename... Types>
SoA<Types...>::SoA(const i32 capacity, Allocator* allocator)
    : allocator_(allocator),
      capacity_(capacity),
      size_(0)
{
    data_ = static_cast<u8*>(BEE_MALLOC(allocator_, sizeof_element_ * capacity_));
    get_array_pointers<Types...>::assign(data_, array_ptrs_, 0, capacity_);
}

template <typename... Types>
SoA<Types...>::SoA(SoA<Types...>&& other) noexcept
{
    move_construct(BEE_FORWARD(other));
}

template <typename... Types>
SoA<Types...>::~SoA()
{
    if (allocator_ == nullptr || data_ == nullptr || size_ <= 0 || capacity_ <= 0)
    {
        return;
    }

    destruct_range(0, size_);
    BEE_FREE(allocator_, data_);

    allocator_ = nullptr;
    capacity_ = 0;
    size_ = 0;
    data_ = nullptr;
}

template <typename... Types>
SoA<Types...>& SoA<Types...>::operator=(SoA<Types...>&& other) noexcept
{
    move_construct(BEE_FORWARD(other));
    return *this;
}

template <typename... Types>
void SoA<Types...>::move_construct(SoA<Types...>&& other) noexcept
{
    destruct_range(0, size_);
    allocator_ = other.allocator_;
    capacity_ = other.capacity_;
    size_ = other.size_;
    data_ = other.data_;
    memcpy(array_ptrs_, other.array_ptrs_, sizeof(u8*) * type_count_);

    other.allocator_ = nullptr;
    other.capacity_ = 0;
    other.size_ = 0;
    other.data_ = nullptr;
}

template <typename... Types>
template <i32 Index>
get_type_at_index_t<Index, Types...>* SoA<Types...>::get()
{
    using array_t = get_type_at_index_t<Index, Types...>;
    static_assert(Index < type_count_, "SoA: Invalid type index");
    return reinterpret_cast<array_t*>(array_ptrs_[Index]);
}

template <typename... Types>
template <i32 Index>
const get_type_at_index_t<Index, Types...>* SoA<Types...>::get() const
{
    return get<Index>();
}

template <typename... Types>
template <typename ArrayType>
ArrayType* SoA<Types...>::get()
{
    static constexpr auto index = get_index_of_type_v<ArrayType, Types...>;
    static_assert(index < type_count_, "SoA: Invalid type index");
    return reinterpret_cast<ArrayType*>(array_ptrs_[index]);
}

template <typename... Types>
template <typename ArrayType>
const ArrayType* SoA<Types...>::get() const
{
    static constexpr auto index = get_index_of_type_v<ArrayType, Types...>;
    static_assert(index < type_count_, "SoA: Invalid type index");
    return reinterpret_cast<ArrayType*>(array_ptrs_[index]);
}

template <typename... Types>
void SoA<Types...>::push_back(const Types&... values)
{
    if (BEE_FAIL_F(size_ + 1 <= capacity_, "SoA: size exceeded storage capacity (%d >= %d)", size_ + 1, capacity_))
    {
        return;
    }

    // Push back a value into all arrays
    column_constructor<Types...>::construct(array_ptrs_, 0, size_, values...);
    ++size_;
}

template <typename... Types>
void SoA<Types...>::push_back_no_construct()
{
    if (BEE_FAIL_F(size_ + 1 <= capacity_, "SoA: size exceeded storage capacity (%d >= %d)", size_ + 1, capacity_))
    {
        return;
    }

    ++size_;
}

template <typename... Types>
void SoA<Types...>::pop_back()
{
    if (BEE_FAIL_F(!empty(), "SoA: popping the back of an empty set of arrays"))
    {
        return;
    }

    destruct_range(size_ - 1, 1);
    --size_;
}


template <typename... Types>
void SoA<Types...>::pop_back_no_destruct()
{
    if (BEE_FAIL_F(!empty(), "SoA: popping the back of an empty set of arrays"))
    {
        return;
    }

    --size_;
}

template <typename... Types>
void SoA<Types...>::destruct_range(const i32 offset, const i32 length)
{
    if (BEE_FAIL_F(offset + length <= size_ && offset >= 0, "SoA: Invalid offset for destruct range"))
    {
        return;
    }

    for (int elem_idx = offset; elem_idx < offset + length; ++elem_idx)
    {
        column_destructor<Types...>::destroy(data_, elem_idx, capacity_);
    }
}

template <typename... Types>
void SoA<Types...>::clear()
{
    destruct_range(0, size_);
    size_ = 0;
}


} // namespace bee