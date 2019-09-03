/*
 *  RingBuffer.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */
#include "Bee/Core/Containers/RingBuffer.hpp"

// memcpy
#include <string.h>


namespace bee {


RingBuffer::RingBuffer(const i32 max_size, Allocator* allocator)
    : max_size_(max_size),
      current_read_pos_(0),
      current_write_pos_(0),
      size_(0),
      data_(static_cast<uint8_t*>(BEE_MALLOC(allocator, sign_cast<u32>(max_size)))),
      allocator_(allocator)
{
    BEE_ASSERT_F(max_size >= 0, "RingBuffer: `max_size` must be >= 0");
}

RingBuffer::~RingBuffer()
{
    // handle potential move
    if (allocator_ == nullptr)
    {
        return;
    }
    BEE_FREE(allocator_, data_);
}

RingBuffer::RingBuffer(RingBuffer&& other) noexcept
    : max_size_(other.max_size_),
      current_read_pos_(other.current_read_pos_),
      current_write_pos_(other.current_write_pos_),
      size_(other.size_),
      data_(other.data_),
      allocator_(other.allocator_)
{
    other.reset();
    other.max_size_ = 0;
    other.allocator_ = nullptr;
    other.data_ = nullptr;
}

RingBuffer& RingBuffer::operator=(RingBuffer&& other) noexcept
{
    max_size_ = other.max_size_;
    allocator_ = other.allocator_;
    current_read_pos_ = other.current_read_pos_;
    current_write_pos_ = other.current_write_pos_;
    size_ = other.size_;
    data_ = other.data_;

    other.reset();
    other.max_size_ = 0;
    other.allocator_ = nullptr;
    other.data_ = nullptr;

    return *this;
}

bool RingBuffer::write(const void* data, const i32 num_bytes_to_write)
{
    BEE_ASSERT_F(num_bytes_to_write >= 0, "RingBuffer: num_bytes_to_write cannot be negative");

    if (num_bytes_to_write == 0 || full())
    {
        return false; // invalid write data
    }

    const auto write_src = static_cast<const uint8_t*>(data);
    const auto write_range_end = current_write_pos_ + num_bytes_to_write;

    // Less than capacity - write in one operation
    if (write_range_end <= max_size_)
    {
        memcpy(data_ + current_write_pos_, write_src, sign_cast<size_t>(num_bytes_to_write));
        current_write_pos_ = (current_write_pos_ + num_bytes_to_write) % max_size_;
    }
    else
    {
        // write in two operations to wrap around
        const auto first_size = max_size_ - current_write_pos_;
        const auto second_size = num_bytes_to_write - first_size;
        memcpy(data_ + current_write_pos_, write_src, sign_cast<size_t>(first_size));
        memcpy(data_, write_src + first_size, sign_cast<size_t>(second_size));
        current_write_pos_ = second_size;
    }

    size_ += num_bytes_to_write;
    return true;
}

bool RingBuffer::read(void* data, const i32 num_bytes_to_read)
{
    const auto empty_data_or_buffer = num_bytes_to_read == 0 || empty();
    if (empty_data_or_buffer)
    {
        return false;
    }

    auto read_dst = static_cast<uint8_t*>(data);
    const auto read_range_end = current_read_pos_ + num_bytes_to_read;
    // read in one step
    if (read_range_end <= max_size_)
    {
        memcpy(read_dst, data_ + current_read_pos_, sign_cast<size_t>(num_bytes_to_read));
        current_read_pos_ = (current_read_pos_ + num_bytes_to_read) % max_size_;
    }
    else
    {
        // wrap-around, read in two steps
        const auto first_size = max_size_ - current_read_pos_;
        const auto second_size = num_bytes_to_read - first_size;
        memcpy(read_dst, data_ + current_read_pos_, sign_cast<size_t>(first_size));
        memcpy(read_dst + first_size, data_, sign_cast<size_t>(second_size));
        current_read_pos_ = second_size;
    }

    size_ -= num_bytes_to_read;
    return true;
}

bool RingBuffer::peek(void* data, const i32 num_bytes_to_peek) const
{
    const auto empty_data_or_buffer = num_bytes_to_peek == 0 || empty();
    if (empty_data_or_buffer)
    {
        return false;
    }

    auto peek_dst = static_cast<uint8_t*>(data);
    const auto peek_range_end = current_read_pos_ + num_bytes_to_peek;
    // read in one step
    if (peek_range_end <= max_size_)
    {
        memcpy(peek_dst, data_ + current_read_pos_, sign_cast<size_t>(num_bytes_to_peek));
    }
    else
    {
        // wrap-around, read in two steps
        const auto first_size = max_size_ - current_read_pos_;
        const auto second_size = num_bytes_to_peek - first_size;
        memcpy(peek_dst, data_ + current_read_pos_, sign_cast<size_t>(first_size));
        memcpy(peek_dst + first_size, data_, sign_cast<size_t>(second_size));
    }

    return true;
}

void RingBuffer::reset()
{
    current_read_pos_ = 0;
    current_write_pos_ = 0;
    size_ = 0;
}


} // namespace bee