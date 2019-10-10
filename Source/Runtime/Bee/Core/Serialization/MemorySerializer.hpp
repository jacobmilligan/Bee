/*
 *  MemorySerializer.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Serialization/Serialization.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

namespace bee {


class BEE_CORE_API MemorySerializer : public Serializer
{
public:
    using buffer_t = DynamicArray<u8>;

    explicit MemorySerializer(buffer_t* buffer);

    // called by the API when beginning serialization of an object
    bool begin();

    // called by the API when ending serialization of an object
    void end();

    // Called when beginning serialization of an object that has defined a custom BEE_SERIALIZE. This
    // function is mostly useful for serializers reading/writing to text formats like JSON where it's
    // important to know when an object should begin and end. `type_name` is the name of the associated
    // struct or class
    void convert_begin_type(const char* type_name);

    // Called when ending the serialization of an object with a custom BEE_SERIALIZE defined.
    void convert_end_type();

    // Called each time a value is serialized - this is where most of the work is done and can
    // be specialized for different types. `name` is the value's variable name in the C++ code
    template <typename T>
    void convert(T* value, const char* name);

    template <typename T, ContainerMode Mode>
    inline void convert(Array<T, Mode>* array, const char* name)
    {
        auto size = array->size();
        serialize_type(this, &size, "array_size");
        if (mode() == SerializerMode::reading)
        {
            array->resize(size);
        }
        convert_cbuffer(array->data(), array->size(), name);
    }

    // Convert a hash map
    template <
        typename        KeyType,
        typename        ValueType,
        ContainerMode   Mode,
        typename        Hasher,
        typename        KeyEqual
    >
    void convert(HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>* map, const char* name)
    {
        auto size = map->size();
        serialize_type(this, &size, "hashmap_size");

        if (mode() == SerializerMode::reading)
        {
            for (int i = 0; i < size; ++i)
            {
                KeyValuePair<KeyType, ValueType> keyval{};
                serialize_type(this, &keyval.key, name);
                serialize_type(this, &keyval.value, name);
                map->insert(keyval);
            }
        }
        else
        {
            for (auto& elem : *map)
            {
                serialize_type(this, &elem.key, name);
                serialize_type(this, &elem.value, name);
            }
        }
    }

    // Convert a String container
    void convert(bee::String* string, const char* name);

    // Convert a Path container
    void convert(Path* path, const char* name);


    // Called when serializing an array
    template <typename T>
    void convert_cbuffer(T* array, const i32 size, const char* name);

    // Called when converting a string of characters
    void convert_cstr(char* string, i32 size, const char* name);
private:
    i32         offset_ { 0 };
    buffer_t*   buffer_ { nullptr };

    inline void* data()
    {
        return buffer_->data() + offset_;
    }
};

template <typename T>
void MemorySerializer::convert(T* value, const char* /* name */)
{
    assert_trivial<T>();

    const auto byte_size = sizeof(T);

    if (mode() == SerializerMode::reading)
    {
        memcpy(value, buffer_->data() + offset_, byte_size);
    }
    else
    {
        buffer_->append(byte_size, 0);
        memcpy(buffer_->data() + offset_, value, byte_size);
    }

    offset_ += sizeof(T);
}

template <typename T>
void MemorySerializer::convert_cbuffer(T* array, const i32 size, const char* /* name */)
{
    const auto byte_size = sizeof(T) * size;

    if (mode() == SerializerMode::writing)
    {
        buffer_->append(byte_size, 0);
    }

    for (int elem_idx = 0; elem_idx < size; ++elem_idx)
    {
        serialize_type(this, &array[elem_idx], "");
    }
}


} // namespace bee