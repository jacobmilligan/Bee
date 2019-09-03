/*
 *  MemorySerializer.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Serialization/MemorySerializer.hpp"

namespace bee {


MemorySerializer::MemorySerializer(buffer_t* buffer)
    : buffer_(buffer)
{}

// called by the API when beginning serialization of an object
bool MemorySerializer::begin()
{
    if (BEE_FAIL_F(buffer_ != nullptr, "MemorySerializer: missing buffer for serialization"))
    {
        return false;
    }

    if (mode() == SerializerMode::reading)
    {
        buffer_->clear();
    }

    offset_ = 0;
    return true;
}

void MemorySerializer::end()
{
    // no-op
}

void MemorySerializer::convert_begin_type(const char* /* type_name */)
{
    // no-op
}

void MemorySerializer::convert_end_type()
{
    // no-op
}

// Convert a String container
void MemorySerializer::convert(bee::String* string, const char* name)
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
void MemorySerializer::convert(Path* path, const char* /* name */)
{
    auto size = path->size();
    serialize_type(this, &size, "path_size");

    if (mode() == SerializerMode::reading)
    {
        StringView view(static_cast<const char*>(data()), size);
        path->clear();
        path->append(view);
    }
    else
    {
        buffer_->append(size, 0);
        memcpy(data(), path->c_str(), size);
    }

    offset_ += size;
}

void MemorySerializer::convert_cstr(char* string, i32 size, const char* /* name */)
{
    const auto byte_size = sizeof(char) * size;

    if (mode() == SerializerMode::reading)
    {
        memcpy(string, buffer_->data() + offset_, byte_size);
    }
    else
    {
        buffer_->append(byte_size, 0);
        memcpy(buffer_->data() + offset_, string, byte_size);
    }

    offset_ += byte_size;
}


} // namespace bee