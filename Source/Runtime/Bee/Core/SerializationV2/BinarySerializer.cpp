/*
 *  BinarySerializer.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/SerializationV2/BinarySerializer.hpp"
#include "Bee/Core/IO.hpp"

namespace bee {


bool BinarySerializer::begin()
{
    if (mode == SerializerMode::reading)
    {
        read_offset = 0;
    }
    return true;
}

void BinarySerializer::end()
{
    // no-op
}

void BinarySerializer::begin_object(i32* member_count)
{
    serialize_bytes(member_count, sizeof(i32));
}

void BinarySerializer::end_object()
{
    // no-op
}

void BinarySerializer::begin_array(i32* count)
{
    serialize_bytes(count, sizeof(i32));
}

void BinarySerializer::end_array()
{
    // no-op
}

void BinarySerializer::serialize_key(String* key)
{
    int size = key->size();
    serialize_fundamental(&size);

    if (mode == SerializerMode::reading)
    {
        key->resize(size);
    }

    serialize_bytes(key->data(), sizeof(char) * key->size());
}

void BinarySerializer::serialize_string(io::StringStream* stream)
{
    int size = stream->size();
    serialize_fundamental(&size);

    if (mode == SerializerMode::writing)
    {
        array->append({reinterpret_cast<const u8*>(stream->c_str()), stream->size() });
    }
    else
    {
        stream->read(array->data() + read_offset, size);
        read_offset = math::min(read_offset + size, array->size());
    }
}

//void BinarySerializer::serialize_enum(const EnumType* type, u8* data)
//{
//    serialize_type(this, type->constants[0].underlying_type, data);
//}

void BinarySerializer::serialize_bytes(void* data, const i32 size)
{
    if (mode == SerializerMode::writing)
    {
        array->append({ static_cast<const u8*>(data), size });
    }
    else
    {
        memcpy(data, array->data() + read_offset, size);
        read_offset = math::min(read_offset + size, array->size());
    }
}

void BinarySerializer::serialize_fundamental(bool* data)
{
    serialize_bytes(data, sizeof(bool));
}

void BinarySerializer::serialize_fundamental(char* data)
{
    serialize_bytes(data, sizeof(char));
}

void BinarySerializer::serialize_fundamental(float* data)
{
    serialize_bytes(data, sizeof(float));
}

void BinarySerializer::serialize_fundamental(double* data)
{
    serialize_bytes(data, sizeof(double));
}

void BinarySerializer::serialize_fundamental(u8* data)
{
    serialize_bytes(data, sizeof(u8));
}

void BinarySerializer::serialize_fundamental(u16* data)
{
    serialize_bytes(data, sizeof(u16));
}

void BinarySerializer::serialize_fundamental(u32* data)
{
    serialize_bytes(data, sizeof(u32));
}

void BinarySerializer::serialize_fundamental(u64* data)
{
    serialize_bytes(data, sizeof(u64));
}

void BinarySerializer::serialize_fundamental(i8* data)
{
    serialize_bytes(data, sizeof(i8));
}

void BinarySerializer::serialize_fundamental(i16* data)
{
    serialize_bytes(data, sizeof(i16));
}

void BinarySerializer::serialize_fundamental(i32* data)
{
    serialize_bytes(data, sizeof(i32));
}

void BinarySerializer::serialize_fundamental(i64* data)
{
    serialize_bytes(data, sizeof(i64));
}


} // namespace bee