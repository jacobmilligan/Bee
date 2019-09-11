/*
 *  Array.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Container.hpp"
#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Span.hpp"

#include <initializer_list>
#include <string.h>

namespace bee {


template <typename T, ContainerMode Mode>
class Array
{
public:
    static constexpr ContainerMode mode = Mode;
    using value_t                       = T;
    using array_t                       = Array<T, Mode>;

    Array() noexcept;

    explicit Array(Allocator* allocator) noexcept;

    explicit Array(i32 capacity, Allocator* allocator = system_allocator()) noexcept;

    Array(i32 size, const T& value, Allocator* allocator = system_allocator()) noexcept;

    explicit Array(const Span<const T>& span, Allocator* allocator = system_allocator()) noexcept;

    Array(std::initializer_list<T> init, Allocator* allocator = system_allocator()) noexcept;

    static Array<T, Mode> with_size(i32 size, Allocator* allocator = system_allocator()) noexcept;

    static Array<T, Mode> with_size(i32 size, const T& value, Allocator* allocator = system_allocator()) noexcept;

    Array(const Array& other) noexcept;

    Array(Array&& other) noexcept;

    ~Array();

    Array& operator=(const Array& other) noexcept;

    Array& operator=(Array&& other) noexcept;

    T& operator[](i32 index);

    const T& operator[](i32 index) const;

    T& back();

    const T& back() const;

    T* data() noexcept;

    const T* data() const noexcept;

    inline bool empty() const noexcept
    {
        return size_ == 0;
    }

    inline i32 size() const noexcept
    {
        return size_;
    }

    inline i32 capacity() const noexcept
    {
        return capacity_;
    }

    inline const Span<const T> const_span() const
    {
        return make_const_span(data(), size());
    }

    inline Span<T> span()
    {
        return make_span(data(), size());
    }

    inline Allocator* allocator() const
    {
        return allocator_;
    }

    T* begin();

    const T* begin() const;

    T* end();

    const T* end() const;

    void reserve(i32 amount) noexcept;

    // resize with default-initialized values
    void resize(i32 new_size);

    void resize_no_raii(const i32 new_size);

    void append(i32 count, const T& value);

    void append(const Array& other);

    void append(const Span<T>& other);

    void append(const Span<const T>& other);

    void clear() noexcept;

    void destruct_range(i32 offset, i32 count);

    void push_back(const T& value);

    void push_back(T&& value);

    void push_back_no_construct();

    void pop_back();

    void pop_back_no_destruct();

    template <class... Args>
    void emplace_back(Args&&... args);

    void erase(const i32 index);

    void copy(const i32 offset, const T* data_begin, const T* data_end);

    void fill_range(const i32 offset, const i32 count, const T& value);

    void fill_uninitialized_range(const i32 offset, const i32 count, const T& value);

    void move_replace_no_destruct(Array<T, Mode>&& other);

    void shrink_to_fit();

private:
    i32         size_ { 0 };
    i32         capacity_ { 0 };
    T*          data_ { nullptr };
    Allocator*  allocator_ { nullptr };

    bool ensure_capacity(const fixed_container_mode_t& fixed_container, i32 new_capacity);

    bool ensure_capacity(const dynamic_container_mode_t& dynamic_container, i32 new_capacity);

    inline constexpr i32 min_capacity(const dynamic_container_mode_t& dynamic_container)
    {
        return 4;
    }

    inline constexpr i32 min_capacity(const fixed_container_mode_t& fixed_container)
    {
        return 1;
    }

    inline constexpr i32 growth_rate(const dynamic_container_mode_t& dynamic_container)
    {
        return capacity_ * 2;
    }

    inline constexpr i32 growth_rate(const fixed_container_mode_t& fixed_container)
    {
        return capacity_ + 1;
    }

    inline void assert_valid_range() const
    {
        BEE_ASSERT_F(size_ <= capacity_, "Invalid range detected: array size was greater than capacity");
    }

    void copy_construct(const Array<T, Mode>& other);

    void move_construct(Array<T, Mode>& other);

    void move_construct_no_destruct(Array<T, Mode>& other);

    void destroy();
};

template <typename T>
using DynamicArray = Array<T, ContainerMode::dynamic_capacity>;

template <typename T>
using FixedArray = Array<T, ContainerMode::fixed_capacity>;


/*
* Array<T, Mode> implementation
*/

template <typename T, ContainerMode Mode>
Array<T, Mode>::Array() noexcept
    : Array(system_allocator())
{}

template <typename T, ContainerMode Mode>
Array<T, Mode>::Array(Allocator* allocator) noexcept
    : size_(0),
      capacity_(0),
      allocator_(allocator),
      data_(nullptr)
{}

template <typename T, ContainerMode Mode>
Array<T, Mode>::Array(const i32 capacity, Allocator* allocator) noexcept
    : size_(0),
      capacity_(0),
      allocator_(allocator)
{
    reserve(capacity);
}

template <typename T, ContainerMode Mode>
Array<T, Mode>::Array(const i32 size, const T& value, Allocator* allocator) noexcept
    : Array(size, allocator)
{
    BEE_ASSERT_F(size >= 0, "Array: `size` must be >= 0");

    size_ = size;
    for (int value_idx = 0; value_idx < size; ++value_idx)
    {
        new(&data_[value_idx]) T(value);
    }
}

template <typename T, ContainerMode Mode>
Array<T, Mode>::Array(const Span<const T>& span, Allocator* allocator) noexcept
    : Array(span.size(), allocator)
{
    for (const auto& value : span)
    {
        push_back(value);
    }
}

template <typename T, ContainerMode Mode>
Array<T, Mode>::Array(std::initializer_list<T> init, Allocator* allocator) noexcept
    : Array(sign_cast<i32>(init.size()), allocator)
{
    for (auto& value : init)
    {
        push_back(value);
    }
}

template <typename T, ContainerMode Mode>
Array<T, Mode> Array<T, Mode>::with_size(const i32 size, Allocator* allocator) noexcept
{
    Array<T, Mode> array(size, T{}, allocator);
    return std::move(array);
}

template <typename T, ContainerMode Mode>
Array<T, Mode> Array<T, Mode>::with_size(const i32 size, const T& value, Allocator* allocator) noexcept
{
    Array<T, Mode> array(size, value, allocator);
    return std::move(array);
}

template <typename T, ContainerMode Mode>
Array<T, Mode>::Array(const Array<T, Mode>& other) noexcept
{
    copy_construct(other);
}

template <typename T, ContainerMode Mode>
Array<T, Mode>::Array(Array<T, Mode>&& other) noexcept
{
    move_construct(other);
}

template <typename T, ContainerMode Mode>
Array<T, Mode>::~Array()
{
    destroy();
}

template <typename T, ContainerMode Mode>
Array<T, Mode>& Array<T, Mode>::operator=(const Array<T, Mode>& other) noexcept
{
    copy_construct(other);
    return *this;
}

template <typename T, ContainerMode Mode>
Array<T, Mode>& Array<T, Mode>::operator=(Array<T, Mode>&& other) noexcept
{
    move_construct(other);
    return *this;
}

template <typename T, ContainerMode Mode>
T& Array<T, Mode>::operator[](const i32 index)
{
    BEE_ASSERT_F(index < size_, "Attempted to access an Array<T> with an out-of-bounds index");
    return data_[index];
}

template <typename T, ContainerMode Mode>
const T& Array<T, Mode>::operator[](const i32 index) const
{
    BEE_ASSERT_F(index < size_, "Attempted to access an Array<T> with an out-of-bounds index");
    return data_[index];
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::destroy()
{
    if (allocator_ != nullptr && data_ != nullptr)
    {
        destruct_range(0, size_);
        BEE_FREE(allocator_, data_);
    }

    size_ = 0;
    capacity_ = 0;
    allocator_ = nullptr;
    data_ = nullptr;
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::copy_construct(const Array<T, Mode>& other)
{
    destroy();
    size_ = other.size_;
    capacity_ = other.capacity_;
    allocator_ = other.allocator_;
    data_ = static_cast<T*>(BEE_MALLOC_ALIGNED(allocator_, capacity_ * sizeof(T), alignof(T)));

    int elem_idx = 0;
    for (auto& other_elem : other)
    {
        new (data_ + elem_idx) T(other_elem);
        ++elem_idx;
    }
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::move_construct(Array<T, Mode>& other)
{
    if (&other == this)
    {
        return;
    }

    if (data_ != nullptr)
    {
        destruct_range(0, size_);
    }

    move_construct_no_destruct(other);
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::move_replace_no_destruct(Array<T, Mode>&& other)
{
    move_construct_no_destruct(other);
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::move_construct_no_destruct(Array<T, Mode>& other)
{
    if (&other == this) {
        return;
    }

    if (allocator_ != nullptr && data_ != nullptr)
    {
        BEE_FREE(allocator_, data_);
    }

    size_ = other.size_;
    capacity_ = other.capacity_;
    allocator_ = other.allocator_;
    data_ = other.data_;
    other.size_ = 0;
    other.capacity_ = 0;
    other.allocator_ = nullptr;
    other.data_ = nullptr;
}

template <typename T, ContainerMode Mode>
T& Array<T, Mode>::back()
{
    return data_[size_ - 1];
}

template <typename T, ContainerMode Mode>
const T& Array<T, Mode>::back() const
{
    return data_[size_ - 1];
}

template <typename T, ContainerMode Mode>
T* Array<T, Mode>::data() noexcept
{
    return data_;
}

template <typename T, ContainerMode Mode>
const T* Array<T, Mode>::data() const noexcept
{
    return data_;
}

template <typename T, ContainerMode Mode>
T* Array<T, Mode>::begin()
{
    return data_;
}

template <typename T, ContainerMode Mode>
const T* Array<T, Mode>::begin() const
{
    return data_;
}

template <typename T, ContainerMode Mode>
T* Array<T, Mode>::end()
{
    return data_ + size_;
}

template <typename T, ContainerMode Mode>
const T* Array<T, Mode>::end() const
{
    return data_ + size_;
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::reserve(const i32 amount) noexcept
{
    // FixedArray can reserve explicitly but can't push back etc.
    ensure_capacity(dynamic_container_mode_t{}, size_ + amount);
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::resize(const i32 new_size)
{
    const auto old_size = size_;

    // FixedArray can be resized explicitly but can't push_back etc.
    ensure_capacity(dynamic_container_mode_t{}, new_size);

    // Change size here so that fill_range works without asserting size too small
    size_ = new_size;

    // Trim the end
    if (new_size < old_size)
    {
        destruct_range(new_size, old_size - new_size);
    }

    // Grow array - append `value` to the end
    if (new_size > old_size)
    {
        // will always be either destructed or uninitialized so this is safe
        assert_valid_range();
        for (int i = old_size; i < new_size; ++i)
        {
            new (data_ + i) T();
        }
    }
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::append(const i32 count, const T& value)
{
    if (count == 0)
    {
        return;
    }

    const auto old_size = size_;
    const auto new_size = size_ + count;

    /*
     * FixedArray can only be resized with explicit call to `resize` and not in call to `append` - appending is
     * semantically the same as calling `push_back` `count` times
     */
    ensure_capacity(container_mode_constant<Mode>{}, new_size);

    // Change size here so that fill_range works without asserting size too small
    size_ = new_size;
    // Appended range is at the end of the array - will always be either destructed or uninitialized so this is safe
    fill_uninitialized_range(old_size, new_size - old_size, value);
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::append(const Array<T, Mode>& other)
{
    append(other.const_span());
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::append(const Span<T>& other)
{
    const const_other = make_const_span(other.data(), other.size());
    append(const_other);
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::append(const Span<const T>& other)
{
    if (other.empty())
    {
        return;
    }

    const auto old_size = size_;
    const auto new_size = size_ + other.size();

    /*
     * FixedArray can only be resized with explicit call to `resize` and not in call to `append` - appending is
     * semantically the same as calling `push_back` `count` times
     */
    ensure_capacity(container_mode_constant<Mode>{}, new_size);

    // Change size here so that fill_range works without asserting size too small
    size_ = new_size;
    copy(old_size, other.begin(), other.end());
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::resize_no_raii(const i32 new_size)
{
    ensure_capacity(dynamic_container_mode_t{}, new_size);
    size_ = new_size;
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::clear() noexcept
{
    assert_valid_range();
    destruct_range(0, size_);
    size_ = 0;
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::destruct_range(const i32 offset, const i32 count)
{
    if (BEE_FAIL(offset >= 0 && offset + count <= capacity_))
    {
        return;
    }

    for (int elem_idx = offset; elem_idx < offset + count; ++elem_idx)
    {
        data_[elem_idx].~T();
    }
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::pop_back()
{
    BEE_ASSERT_F(size_ > 0, "Attempted to pop from the back of an empty Array<T>");
    back().~T();
    --size_;
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::pop_back_no_destruct()
{
    BEE_ASSERT_F(size_ > 0, "Attempted to pop from the back of an empty Array<T>");
    --size_;
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::push_back(const T& value)
{
    const auto new_size = size_ + 1;
    if (!ensure_capacity(container_mode_constant<Mode>{}, new_size))
    {
        return;
    }

    size_ = new_size;
    new (&data_[size_ - 1]) T(value);
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::push_back(T&& value)
{
    const auto new_size = size_ + 1;
    if (!ensure_capacity(container_mode_constant<Mode>{}, new_size)) {
        return;
    }

    size_ = new_size;
    new(&data_[size_ - 1]) T(std::move(value));
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::push_back_no_construct()
{
    const auto new_size = size_ + 1;
    if (!ensure_capacity(container_mode_constant<Mode>{}, new_size))
    {
        return;
    }

    size_ = new_size;
}

template <typename T, ContainerMode Mode>
template <class... Args>
void Array<T, Mode>::emplace_back(Args&&... args)
{
    const auto new_size = size_ + 1;
    if (!ensure_capacity(container_mode_constant<Mode>{}, new_size))
    {
        return;
    }

    size_ = new_size;
    new (&data_[size_ - 1]) T(std::forward<Args>(args)...);
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::erase(const i32 index)
{
    BEE_ASSERT(index < size_);
    data_[index].~T();
    if (index < size_ - 1)
    {
        memmove(data_ + index, data_ + index + 1, sizeof(T) * (size_ - index - 1));
    }
    --size_;
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::copy(const i32 offset, const T* data_begin, const T* data_end)
{
    assert_valid_range();

    const auto count = static_cast<i32>(data_end - data_begin);

    BEE_ASSERT_F(
        offset + count <= size_,
        "Attempted to assign to a buffer with a range and offset larger than the buffer"
    );

    for (int elem_idx = offset; elem_idx < offset + count; ++elem_idx)
    {
        data_[elem_idx] = data_begin[elem_idx - offset];
    }
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::fill_range(const i32 offset, const i32 count, const T& value)
{
    assert_valid_range();

    if (BEE_FAIL(offset + count <= size_))
    {
        return;
    }

    for (int elem_idx = offset; elem_idx < count; ++elem_idx)
    {
        data_[elem_idx] = value;
    }
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::fill_uninitialized_range(const i32 offset, const i32 count, const T& value)
{
    assert_valid_range();

    if (BEE_FAIL(offset + count <= size_))
    {
        return;
    }

    for (int elem_idx = offset; elem_idx < count; ++elem_idx)
    {
        new (&data_[elem_idx]) T(value);
    }
}

template <typename T, ContainerMode Mode>
void Array<T, Mode>::shrink_to_fit()
{
    BEE_ASSERT(capacity_ >= size_);

    if (capacity_ == size_ || data_ == nullptr)
    {
        return;
    }

    T* new_memory = nullptr;

    if (size_ <= 0)
    {
        BEE_FREE(allocator_, data_);
    }
    else
    {
        new_memory = static_cast<T*>(BEE_REALLOC(allocator_, data_, capacity_, size_, alignof(T)));
        BEE_ASSERT_F(new_memory != nullptr, "Failed to reallocate memory when calling Array::shrink_to_fit");
    }

    data_ = new_memory;
    capacity_ = size_;
}

/*
 * Dynamic array implementation
 */
template <typename T, ContainerMode Mode>
bool Array<T, Mode>::ensure_capacity(const dynamic_container_mode_t& dynamic_container, const i32 new_capacity)
{
    if (new_capacity <= capacity_)
    {
        return true;
    }

    // try to grow by growth rate based on container mode and if that's too small just grow to the given capacity
    static constexpr auto container_mode_val = container_mode_constant<Mode>{};
    auto resize_amount = capacity_ > 0 ? growth_rate(container_mode_val) : min_capacity(container_mode_val);

    // min of new_capacity and double_capacity
    if (resize_amount < new_capacity)
    {
        resize_amount = new_capacity;
    }

    // Grow the internal buffer
    BEE_ASSERT_F(allocator_ != nullptr, "Attempted to increase the capacity of an invalid Buffer<T>");
    BEE_ASSERT(resize_amount > capacity_);

    // Allocate new data and copy over the old data to the new allocation if it exists
    auto new_data = BEE_MALLOC_ALIGNED(allocator_, resize_amount * sizeof(T), alignof(T));
    BEE_ASSERT_F(new_data != nullptr, "Failed to reallocate memory while resizing a buffer");

    if (data_ != nullptr)
    {
        memcpy(new_data, data_, capacity_ * sizeof(T));
        // deallocate the old data that was copied
        BEE_FREE(allocator_, data_);
    }

    capacity_ = resize_amount;
    data_ = static_cast<T*>(new_data);
    return true;
}

/*
 * Fixed array implementation
 */
template <typename T, ContainerMode Mode>
bool Array<T, Mode>::ensure_capacity(const fixed_container_mode_t& fixed_container, const i32 new_capacity)
{
    return BEE_CHECK_F(new_capacity <= capacity_, "FixedArray<T>: new_capacity exceeded the fixed capacity of the array");
}


} // namespace bee
