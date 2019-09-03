//
//  StreamSerializer.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 1/06/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Serialization/StreamSerializer.hpp"

namespace bee {


StreamSerializer::StreamSerializer(io::Stream* stream)
    : stream_(stream)
{}

bool StreamSerializer::begin()
{
    stream_->seek(0, io::SeekOrigin::begin);
    return true;
}

void StreamSerializer::end()
{
    // no-op - stream should handle this outside the serializer
}

void StreamSerializer::convert_begin_type(const char* /* type_name */)
{
    // no-op - binary files don't need to scope types and arrays
}

void StreamSerializer::convert_end_type()
{
    // no-op - binary files don't need to scope types and arrays
}

// Convert a String container
void StreamSerializer::convert(bee::String* string, const char* name)
{
    auto size = string->size();
    serialize_type(this, &size, "string_size");
    if (mode() == SerializerMode::reading)
    {
        string->clear();
        string->insert(0, size, '\0');
    }
    convert_cstr(string->data(), size, name);
}

// Convert a Path container
void StreamSerializer::convert(Path* path, const char* name)
{
    auto size = path->size();
    serialize_type(this, &size, "path_size");
    auto temp_string = path->to_string();
    if (mode() == SerializerMode::reading)
    {
        temp_string.clear();
        temp_string.insert(0, size, '\0');
    }
    convert_cstr(temp_string.data(), temp_string.size(), name);
    path->clear();
    path->append(temp_string.view());
}

void StreamSerializer::convert_cstr(char* string, i32 size, const char* /* name */)
{
    if (mode() == SerializerMode::reading)
    {
        stream_->read(string, sizeof(char) * size);
    }
    else
    {
        stream_->write(string, sizeof(char) * size);
    }
}


} // namespace bee
