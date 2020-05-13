/*
 *  IO.inl
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include <stdarg.h>
#include <string.h>

namespace bee {
namespace io {


template <>
inline i32 write_fmt(String* dst, const char* format, ...)
{
    BEE_ASSERT(dst != nullptr);

    va_list args;
    va_start(args, format);

    const auto length = str::system_snprintf(nullptr, 0, format, args);
    const auto old_dst_size = dst->size();
    dst->resize(old_dst_size + length);
    // include null-terminator
    str::system_snprintf(dst->data() + old_dst_size, sign_cast<size_t>(length + 1), format, args);

    va_end(args);

    return length;
}

template <typename T, ContainerMode ArrayType>
inline i32 write_fmt(Array<T, ArrayType>* dst, const char* format, ...)
{
    BEE_ASSERT(dst != nullptr);

    va_list args;
    va_start(args, format);

    const auto length = str::system_snprintf(nullptr, 0, format, args);
    const auto old_dst_size = dst->size();
    dst->append(length, '\0');
    // include null-terminator
    str::system_snprintf(dst->data() + old_dst_size, sign_cast<size_t>(length + 1), format, args);

    va_end(args);

    return length;
}

template <>
inline i32 write(String* dst, const Span<const u8>& data)
{
    BEE_ASSERT(dst != nullptr);

    const auto old_size = dst->size();
    dst->insert(dst->size(), data.size(), '\0');
    memcpy(dst->data() + old_size, data.data(), data.size());
    return dst->size() - old_size;
}

template <ContainerMode ArrayType>
inline i32 write(Array<u8, ArrayType>* dst, const Span<const u8>& data)
{
    BEE_ASSERT(dst != nullptr);
    const auto old_size = dst->size();
    dst->append(data.size(), 0);
    memcpy(dst->data() + old_size, data.begin(), data.size());
    return dst->size() - old_size;
}




} // namespace io
} // namespace bee
