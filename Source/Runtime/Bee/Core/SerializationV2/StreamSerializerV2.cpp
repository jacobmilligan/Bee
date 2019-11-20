/*
 *  StreamSerializer.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/SerializationV2/StreamSerializerV2.hpp"

namespace bee {


bool StreamSerializerV2::begin()
{
    stream->seek(0, io::SeekOrigin::begin);
    return true;
}

void StreamSerializerV2::end()
{
    // no-op
}

void StreamSerializerV2::begin_record(const RecordType* type)
{
    // no-op
}

void StreamSerializerV2::end_record()
{
    // no-op
}

void StreamSerializerV2::serialize_field(const Field& field)
{
    // no-op
}

void StreamSerializerV2::serialize_enum(const bee::EnumType* type, u8* data)
{
    serialize_type(this, type->constants[0].underlying_type, data);
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


#define IMPLEMENT_BUILTIN(type) void StreamSerializerV2::serialize_fundamental(type* data)  \
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