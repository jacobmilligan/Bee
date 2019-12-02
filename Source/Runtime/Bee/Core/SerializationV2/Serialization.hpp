/*
 *  Serialization.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/ReflectionV2.hpp"

namespace bee {


enum class SerializerMode
{
    reading,
    writing
};

enum class SerializerFormat
{
    unknown,
    binary,
    text
};


struct BEE_CORE_API Serializer
{
    SerializerMode          mode { SerializerMode::reading };
    const SerializerFormat  format {SerializerFormat::unknown };

    Serializer(const SerializerFormat serialized_format);

    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual void begin_record(const RecordType* type) = 0;
    virtual void end_record() = 0;
    virtual void serialize_field(const Field& field) = 0;
    virtual void serialize_bytes(void* data, const i32 size) = 0;
    virtual void serialize_fundamental(bool* data) = 0;
    virtual void serialize_fundamental(char* data) = 0;
    virtual void serialize_fundamental(float* data) = 0;
    virtual void serialize_fundamental(double* data) = 0;
    virtual void serialize_fundamental(bee::u8* data) = 0;
    virtual void serialize_fundamental(bee::u16* data) = 0;
    virtual void serialize_fundamental(bee::u32* data) = 0;
    virtual void serialize_fundamental(bee::u64* data) = 0;
    virtual void serialize_fundamental(bee::i8* data) = 0;
    virtual void serialize_fundamental(bee::i16* data) = 0;
    virtual void serialize_fundamental(bee::i32* data) = 0;
    virtual void serialize_fundamental(bee::i64* data) = 0;
};


BEE_CORE_API void serialize_type(const i32 serialized_version, Serializer* serializer, const Type* type, u8* data);


struct FieldHeader
{
    u32 type_hash { 0 };
    u32 field_hash { 0 };

    FieldHeader() = default;

    explicit FieldHeader(const Field& field)
        : type_hash(field.type->hash),
          field_hash(field.hash)
    {}
};

inline bool operator==(const FieldHeader& lhs, const FieldHeader& rhs)
{
    return lhs.type_hash == rhs.type_hash && lhs.field_hash == rhs.field_hash;
}

inline bool operator!=(const FieldHeader& lhs, const FieldHeader& rhs)
{
    return !(lhs == rhs);
}

class BEE_CORE_API SerializationBuilder
{
public:
    explicit SerializationBuilder(Serializer* new_serializer, const Type* type, u8* data);

    SerializationBuilder& version(const i32 value);

    template <typename T, typename FieldType>
    SerializationBuilder& add(const i32 version_added, FieldType T::* field)
    {
        return add(version_added, limits::max<i32>(), field);
    }

    template <typename T, typename FieldType>
    SerializationBuilder& add(const i32 version_added, const i32 version_removed, FieldType T::* field)
    {
        const auto parent_type = get_type<T>();
        const auto field_type = get_type<FieldType>();

        if (BEE_FAIL_F(parent_type == serialized_type_, "Tried to serialize field `%s` which belongs to type `%s` but the serializer is currently serializing type `%s`", field_type->name, parent_type->name, serialized_type_->name))
        {
            return *this;
        }

        if (version_ < version_added || version_ >= version_removed)
        {
            return *this;
        }

        auto field_ptr = &(reinterpret_cast<T*>(serialized_data_)->*field);
        serialize_type(version_, serializer_, field_type, reinterpret_cast<u8*>(field_ptr));
        return *this;
    }

//    SerializationBuilder& add_bytes(const i32 version_added, const size_t offset, const size_t size)
//    {
//        return add_bytes(version_added, limits::max<i32>(), offset, size);
//    }
//
//    SerializationBuilder& add_bytes(const i32 version_added, const i32 version_removed, const size_t offset, const size_t size)
//    {
//        if (BEE_FAIL_F(offset + size <= serialized_type->size, "failed to serialize bytes because offset + size (%zu) was greater than the size of the serialized type `%s` (%zu)", offset + size, serialized_type->name, serialized_type->size))
//        {
//            return *this;
//        }
//
//        if (base_serializer->version < version_added || base_serializer->version >= version_removed)
//        {
//            return *this;
//        }
//
//        serialize_bytes(offset, size)
//    }

    template <typename FieldType>
    SerializationBuilder& remove(const i32 version_added, const i32 version_removed, const FieldType& default_value)
    {
        const auto field_type = get_type<FieldType>();
        if (version_ < version_added || version_ >= version_removed)
        {
            return *this;
        }

        FieldType removed_data;

        if (serializer_->mode == SerializerMode::writing)
        {
            memcpy_or_assign(&removed_data, &default_value, 1);
        }

        serialize_type(version_, serializer_, field_type, reinterpret_cast<u8*>(&removed_data));
        return *this;
    }

private:
    i32         version_ { 1 };
    Serializer* serializer_ { nullptr };
    const Type* serialized_type_ { nullptr };
    u8*         serialized_data_ { nullptr };
};

template <typename DataType>
inline void serialize(const SerializerMode mode, Serializer* serializer, DataType* data)
{
    BEE_ASSERT_F(serializer->format != SerializerFormat::unknown, "Serializer has an invalid kind");

    const auto type = get_type<DataType>();
    if (BEE_FAIL_F(type->kind != TypeKind::unknown, "`DataType` is not marked for reflection - use BEE_REFLECT() on the types declaration"))
    {
        return;
    }

    serializer->mode = mode;

    if (BEE_FAIL_F(serializer->begin(), "Failed to initialize serialization"))
    {
        return;
    }

    serialize_type(type->serialized_version, serializer, type, reinterpret_cast<u8*>(data));
    serializer->end();
}



} // namespace bee