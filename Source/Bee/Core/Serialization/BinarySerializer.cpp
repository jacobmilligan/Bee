/*
 *  BinarySerializer.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Serialization/BinarySerializer.hpp"

namespace bee {


size_t BinarySerializer::offset()
{
    return mode == SerializerMode::reading ? read_offset : array->size();
}

size_t BinarySerializer::capacity()
{
    return mode == SerializerMode::reading ? array->size() : limits::max<size_t>();
}


bool BinarySerializer::begin()
{
    if (mode == SerializerMode::reading)
    {
        read_offset = 0;
    }
    else
    {
        array->clear();
    }

    return true;
}

void BinarySerializer::end()
{
    // no-op
}

void BinarySerializer::begin_object(i32* member_count)
{
    serialize_fundamental(member_count);
}

void BinarySerializer::end_object()
{
    // no-op
}

void BinarySerializer::begin_array(i32* count)
{
    serialize_fundamental(count);
}

void BinarySerializer::end_array()
{
    // no-op
}

static void serialize_buffer(BinarySerializer* serializer, void* data, const i32 size)
{
    if (serializer->mode == SerializerMode::writing)
    {
        serializer->array->append({ static_cast<const u8*>(data), size });
    }
    else
    {
        memcpy(data, serializer->array->data() + serializer->read_offset, size);
        serializer->read_offset = math::min(serializer->read_offset + size, serializer->array->size());
    }
}

void BinarySerializer::serialize_key(String* key)
{
    int size = key->size();
    serialize_fundamental(&size);

    if (mode == SerializerMode::reading)
    {
        key->resize(size);
    }

    serialize_buffer(this, key->data(), sizeof(char) * key->size());
}

void BinarySerializer::begin_text(i32* length)
{
    serialize_fundamental(length);
}

void BinarySerializer::end_text(char* buffer, const i32 size, const i32 capacity)
{
    if (mode == SerializerMode::writing)
    {
        array->append({ reinterpret_cast<const u8*>(buffer), size });
    }
    else
    {
        // Only read what the buffer can store, but still increment read offset by the serialized size to avoid
        // messing up the data
        BEE_ASSERT(array->size() - read_offset >= size);
        str::copy(buffer, capacity, reinterpret_cast<const char*>(array->data() + read_offset), size);
        read_offset = math::min(read_offset + size, array->size());
    }
}

void BinarySerializer::begin_bytes(i32* size)
{
    serialize_fundamental(size);
}

void BinarySerializer::end_bytes(u8* buffer, const i32 size)
{
    if (mode == SerializerMode::writing)
    {
        array->append({ buffer, size });
    }
    else
    {
        memcpy(buffer, array->data() + read_offset, size);
        read_offset = math::min(read_offset + size, array->size());
    }
}


void BinarySerializer::serialize_fundamental(bool* data)
{
    serialize_buffer(this, data, sizeof(bool));
}

void BinarySerializer::serialize_fundamental(char* data)
{
    serialize_buffer(this, data, sizeof(char));
}

void BinarySerializer::serialize_fundamental(float* data)
{
    serialize_buffer(this, data, sizeof(float));
}

void BinarySerializer::serialize_fundamental(double* data)
{
    serialize_buffer(this, data, sizeof(double));
}

void BinarySerializer::serialize_fundamental(u8* data)
{
    serialize_buffer(this, data, sizeof(u8));
}

void BinarySerializer::serialize_fundamental(u16* data)
{
    serialize_buffer(this, data, sizeof(u16));
}

void BinarySerializer::serialize_fundamental(u32* data)
{
    serialize_buffer(this, data, sizeof(u32));
}

void BinarySerializer::serialize_fundamental(u64* data)
{
    serialize_buffer(this, data, sizeof(u64));
}

void BinarySerializer::serialize_fundamental(i8* data)
{
    serialize_buffer(this, data, sizeof(i8));
}

void BinarySerializer::serialize_fundamental(i16* data)
{
    serialize_buffer(this, data, sizeof(i16));
}

void BinarySerializer::serialize_fundamental(i32* data)
{
    serialize_buffer(this, data, sizeof(i32));
}

void BinarySerializer::serialize_fundamental(i64* data)
{
    serialize_buffer(this, data, sizeof(i64));
}

void BinarySerializer::serialize_fundamental(u128* data)
{
    serialize_buffer(this, data, sizeof(u128));
}


} // namespace bee