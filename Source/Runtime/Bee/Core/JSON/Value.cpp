/*
 *  Value.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/JSON/Value.hpp"
#include "Bee/Core/Math/Math.hpp"

#include <utility>

namespace bee {
namespace json {


ValueAllocator::ValueAllocator(const AllocationMode allocation_mode, const i32 initial_capacity)
    : allocation_mode_(allocation_mode),
      size_(0),
      buffer_(initial_capacity * sizeof(u8))
{}

ValueAllocator::ValueAllocator(ValueAllocator&& other) noexcept
{
    size_ = other.size_;
    allocation_mode_ = other.allocation_mode_;
    buffer_.move_replace_no_destruct(std::move(other.buffer_));

    other.allocation_mode_ = AllocationMode::fixed;
    other.size_ = 0;
}

ValueAllocator& ValueAllocator::operator=(ValueAllocator&& other) noexcept
{
    size_ = other.size_;
    allocation_mode_ = other.allocation_mode_;
    buffer_.move_replace_no_destruct(std::move(other.buffer_));

    other.allocation_mode_ = AllocationMode::fixed;
    other.size_ = 0;

    return *this;
}

bool ValueAllocator::ensure_capacity(const i32 new_capacity)
{
    if (new_capacity < buffer_.capacity()) {
        return true;
    }

    // fixed size stack allocator can only grow if no memory has been allocated yet
    if (allocation_mode_ == AllocationMode::fixed) {
        return false;
    }

    buffer_.resize(new_capacity);
    return true;
}

ValueHandle ValueAllocator::allocate(const ValueType type, const void* data, const i32 size)
{
    BEE_ASSERT_F(size <= sizeof(ValueData::Contents), "json::ValueAllocator: invalid data size");

    const auto has_capacity = ensure_capacity(size_ + sizeof(ValueData));
    if (BEE_FAIL_F(has_capacity, "json::ValueAllocator: cannot grow internal buffer")) {
        return ValueHandle{};
    }

    ValueData value{};
    value.type = type;
    value.contents.integer_value = 0;
    memcpy(&value.contents, data, sign_cast<size_t>(size));
    memcpy(buffer_.data() + size_, &value, sizeof(ValueData));

    const auto old_size = size_;
    size_ += sizeof(ValueData);

    return ValueHandle {old_size};
}

u8* ValueAllocator::reserve(const i32 size)
{
    const auto has_capacity = ensure_capacity(size_ + size);
    if (BEE_FAIL_F(has_capacity, "json::ValueAllocator: cannot grow internal buffer")) {
        return nullptr;
    }

    const auto old_size = size_;
    size_ += size;
    return buffer_.data() + old_size;
}

const ValueData* ValueAllocator::get(const ValueHandle& handle) const
{
    return get_internal(handle);
}

ValueData* ValueAllocator::get(const ValueHandle& handle)
{
    return const_cast<ValueData*>(get_internal(handle));
}

const ValueData* ValueAllocator::get_internal(const ValueHandle& handle) const
{
    if (handle.id >= size_ || handle.id >= buffer_.capacity()) {
        return nullptr;
    }

    return reinterpret_cast<const ValueData*>(buffer_.data() + handle.id);
}

/*
ValueHandle ValueAllocator::append(const ValueHandle& root, const ValueType type,
                                   const void* value, const i32 size)
{
    auto root_data = get(root);
    if (root_data == nullptr) {
        return ValueHandle{};
    }

    const auto can_append = root_data->type == ValueType::object
                         || root_data->type == ValueType::array;
    if (BEE_FAIL_F(can_append, "json::ValueAllocator: cannot append values to a root value of that type")) {
        return ValueHandle{};
    }

    const auto alloc_size = sizeof(ValueData) + size;
    const auto new_cursor = memory_end_ + alloc_size;

    if (BEE_FAIL_F(ensure_capacity(new_cursor), "json::ValueAllocator: cannot grow internal buffer")) {
        return ValueHandle{};
    }

    // move existing values across to keep data contiguous
    const auto move_src = memory_ + root_data->offset + root_data->size;
    auto move_dst = memory_ + root_data->offset + root_data->size + alloc_size;
    memmove(move_dst, move_src, mesh_count_ - new_cursor);

    ValueData new_value{};
    new_value.type = type;
    new_value.offset = root_data->offset + root_data->size;
    new_value.size = alloc_size;
    new_value.children = 0;

    memcpy(move_src, &new_value, sizeof(ValueData));
    memcpy(move_src + sizeof(ValueData), value, size);

    memory_end_ = new_cursor;
    return { new_value.offset };
}
*/





} // namespace json
} // namespace bee