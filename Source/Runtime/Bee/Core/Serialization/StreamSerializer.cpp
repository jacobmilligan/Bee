/*
 *  StreamSerializer.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Serialization/StreamSerializer.hpp"

namespace bee {


bool StreamSerializer::begin()
{
    stream->seek(0, io::SeekOrigin::begin);
    return true;
}

void StreamSerializer::end()
{
    // no-op
}

void StreamSerializer::begin_object(i32* member_count)
{
    serialize_fundamental(member_count);
}

void StreamSerializer::begin_array(i32* count)
{
    serialize_fundamental(count);
}

void StreamSerializer::serialize_field(const char* name)
{

}

void StreamSerializer::serialize_key(String* key)
{
    int size = key->size();
    serialize_fundamental(&size);

    if (mode == SerializerMode::writing)
    {
        stream->write(key->data(), sizeof(char) * size);
    }
    else
    {
        key->resize(size);
        stream->read(key->data(), sizeof(char) * size);
    }
}

void StreamSerializer::begin_text(i32* length)
{
    serialize_fundamental(length);
}

void StreamSerializer::end_text(char* buffer, const i32 size, const i32 capacity)
{
    if (mode == SerializerMode::writing)
    {
        stream->write(buffer, size);
    }
    else
    {
        stream->read(buffer, math::min(size, capacity));
    }
}

//void StreamSerializer::serialize_enum(const bee::EnumType* type, u8* data)
//{
//    serialize_type(this, type->constants[0].underlying_type, data);
//}

void StreamSerializer::serialize_bytes(void* data, const i32 size)
{
    if (mode == SerializerMode::reading)
    {
        stream->read(data, size);
    }
    else
    {
        stream->write(data, size);
    }
}

template <typename T>
void stream_serialize_fundamental(const SerializerMode mode, io::Stream* stream, T* data)
{
    if (mode == SerializerMode::reading)
    {
        stream->read(data, sizeof(T));
    }
    else
    {
        stream->write(data, sizeof(T));
    }
}


#define IMPLEMENT_BUILTIN(type) void StreamSerializer::serialize_fundamental(type* data)  \
    {                                                                                       \
        stream_serialize_fundamental(mode, stream, data);                                   \
    }

IMPLEMENT_BUILTIN(bool)
IMPLEMENT_BUILTIN(char)
IMPLEMENT_BUILTIN(float)
IMPLEMENT_BUILTIN(double)
IMPLEMENT_BUILTIN(u8)
IMPLEMENT_BUILTIN(u16)
IMPLEMENT_BUILTIN(u32)
IMPLEMENT_BUILTIN(u64)
IMPLEMENT_BUILTIN(i8)
IMPLEMENT_BUILTIN(i16)
IMPLEMENT_BUILTIN(i32)
IMPLEMENT_BUILTIN(i64)


} // namespace bee