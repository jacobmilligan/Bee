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


/*
 * This macro must be updated to mirror BEE_BUILTIN_TYPES whenever it's updated
 */
#define BEE_SERIALIZER_INTERFACE(serializer_kind)                               \
    static constexpr auto kind = bee::SerializerKind::serializer_kind;          \
    bool begin();                                                               \
    void end();                                                                 \
    void begin_record(const bee::RecordType* type);                             \
    void end_record();                                                          \
    void serialize_field(const bee::Field& field);                              \
    void serialize_enum(const bee::EnumType* type, u8* data);                   \
    void serialize_fundamental(bool* data);                                     \
    void serialize_fundamental(char* data);                                     \
    void serialize_fundamental(float* data);                                    \
    void serialize_fundamental(double* data);                                   \
    void serialize_fundamental(bee::u8* data);                                  \
    void serialize_fundamental(bee::u16* data);                                 \
    void serialize_fundamental(bee::u32* data);                                 \
    void serialize_fundamental(bee::u64* data);                                 \
    void serialize_fundamental(bee::i8* data);                                  \
    void serialize_fundamental(bee::i16* data);                                 \
    void serialize_fundamental(bee::i32* data);                                 \
    void serialize_fundamental(bee::i64* data);


enum class SerializerMode
{
    reading,
    writing
};

enum class SerializerKind
{
    binary,
    text
};


struct Serializer
{
    i32             version { 0 };
    SerializerMode  mode { SerializerMode::reading };
};


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

struct SerializationBuilder
{
    Serializer*   base_serializer { nullptr };
    const Type*   serialized_type { nullptr };
    u8*           serialized_data { nullptr };

    explicit SerializationBuilder(Serializer* serializer, const Type* type, u8* data)
        : base_serializer(serializer),
          serialized_type(type),
          serialized_data(data)
    {}

    SerializationBuilder& version(const i32 value)
    {
        if (base_serializer->mode == SerializerMode::writing)
        {
            base_serializer->version = value;
        }

        serialize_version(value);
        return *this;
    }

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

        if (BEE_FAIL_F(parent_type == serialized_type, "Tried to serialize field `%s` which belongs to type `%s` but the serializer is currently serializing type `%s`", field_type->name, parent_type->name, serialized_type->name))
        {
            return *this;
        }

        if (base_serializer->version < version_added || base_serializer->version >= version_removed)
        {
            return *this;
        }

        auto field_ptr = &(reinterpret_cast<T*>(serialized_data)->*field);
        serialize_field(field_type, reinterpret_cast<u8*>(field_ptr));
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
        if (base_serializer->version < version_added || base_serializer->version >= version_removed)
        {
            return *this;
        }

        FieldType removed_data;

        if (base_serializer->mode == SerializerMode::writing)
        {
            memcpy_or_assign(&removed_data, &default_value, 1);
        }

        serialize_field(field_type, reinterpret_cast<u8*>(&removed_data));
        return *this;
    }

    virtual void serialize_version(const i32 value) = 0;

    virtual void serialize_field(const Type* type, u8* field_data) = 0;
};


template <typename SerializerType>
struct SerializerWrapper final : public SerializationBuilder
{
    SerializerType* serializer { nullptr };

    SerializerWrapper(SerializerType* new_serializer, const Type* new_type, u8* new_data)
        : SerializationBuilder(new_serializer, new_type, new_data),
          serializer(new_serializer)
    {}

    void serialize_version(const i32 value) override
    {
        static Field version_field(get_type_hash("bee::version"), 0, Qualifier::none, StorageClass::none, "bee::version", get_type<i32>(), {}, 1);

        serializer->serialize_field(version_field);
        serializer->serialize_fundamental(&serializer->version);
    }

    void serialize_field(const Type* type, u8* field_data) override
    {
        serialize_type(serializer, type, field_data);
    }
};


using binary_serializer_constant_t = std::integral_constant<SerializerKind, SerializerKind::binary>;
using text_serializer_constant_t = std::integral_constant<SerializerKind, SerializerKind::text>;

template <typename SerializerType>
void serialize_record(const binary_serializer_constant_t& binary_kind, SerializerType* serializer, const RecordType* type, u8* dst)
{
    int field_count = 0;

    if (serializer->mode == SerializerMode::reading)
    {
        serializer->serialize_fundamental(&field_count);
    }
    else
    {
        field_count = type->fields.size();
        int serialized_field_count = field_count;

        for (const Field& field : type->fields)
        {
            if (field.version_added <= 0)
            {
                --serialized_field_count;
            }
        }

        serializer->serialize_fundamental(&serialized_field_count);
    }


    for (int f = 0; f < field_count; ++f)
    {
        Field* field = nullptr;

        if (serializer->mode == SerializerMode::reading)
        {
            FieldHeader header {};
            serializer->serialize_fundamental(&header.type_hash);
            serializer->serialize_fundamental(&header.field_hash);

            const auto index = container_index_of(type->fields, [&](const Field& decl_field)
            {
                return decl_field.type->hash == header.type_hash && decl_field.hash == header.field_hash;
            });

            if (BEE_FAIL_F(index >= 0, "serialization of record type `%s` failed: detected missing field. The fields may have been renamed or it's type changed", type->name))
            {
                return;
            }

            field = &type->fields[index];
        }
        else
        {
            field = &type->fields[f];
            
            // this is a nonserialized field if version_added is <= 0
            if (field->version_added <= 0)
            {
                continue;
            }

            FieldHeader header(*field);
            serializer->serialize_fundamental(&header.type_hash);
            serializer->serialize_fundamental(&header.field_hash);
        }

        if (serializer->version >= field->version_added && serializer->version < field->version_removed)
        {
            serializer->serialize_field(*field);
            serialize_type(serializer, field->type, dst + field->offset);
        }
    }
}


template <typename SerializerType>
void serialize_record(const text_serializer_constant_t& text_kind, SerializerType* serializer, const RecordType* type, u8* dst)
{
    for (const Field& field : type->fields)
    {
        if (serializer->version >= field.version_added && serializer->version < field.version_removed)
        {
            serializer->serialize_field(field);
            serialize_type(serializer, field.type, dst + field.offset);
        }
    }
}


template <typename SerializerType>
void serialize_type(SerializerType* serializer, const Type* type, u8* dst)
{
    static Field version_field(get_type_hash("bee::version"), 0, Qualifier::none, StorageClass::none, "bee::version", get_type<i32>(), {}, 1);
//    static Field type_field(0, Qualifier::none, StorageClass::none, "bee::version", get_type<i32>(), {}, 1);

    if (type->serialized_version <= 0)
    {
        log_error("Skipping serialization for `%s`: type is not marked for serialization using the `serialized_version` attribute", type->name);
        return;
    }

    // Handle custom serialization
    if ((type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
    {
        SerializerWrapper<SerializerType> builder(serializer, type, dst);
        auto attr = find_attribute(type, "serializer_function", AttributeKind::type);
        if (attr == nullptr || attr->value.type->kind != TypeKind::function)
        {
            log_warning("Skipping serialization for `%s`: custom serialization function was not a valid function type", type->name);
            return;
        }

        reinterpret_cast<const FunctionType*>(attr->value.type)->invoke<void>(&builder);
        return;
    }

    // Otherwise handle automatic serialization
    if (serializer->mode == SerializerMode::writing)
    {
        serializer->version = type->serialized_version;
    }

    switch (type->kind)
    {
        case TypeKind::class_decl:
        case TypeKind::struct_decl:
        case TypeKind::union_decl:
        {
            const auto record_type = reinterpret_cast<const RecordType*>(type);

            serializer->begin_record(record_type);
            serializer->serialize_field(version_field);
            serializer->serialize_fundamental(&serializer->version);
            serialize_record(std::integral_constant<SerializerKind, SerializerType::kind>{}, serializer, record_type, dst);
            serializer->end_record();
            break;
        }
        case TypeKind::enum_decl:
        {
            serializer->serialize_enum(reinterpret_cast<const EnumType*>(type), dst);
            break;
        }
        case TypeKind::fundamental:
        {
#define BEE_SERIALIZE_FUNDAMENTAL(fundamental_kind, serialized_type)                        \
            case FundamentalKind::fundamental_kind:                                         \
            {                                                                               \
                serializer->serialize_fundamental(reinterpret_cast<serialized_type*>(dst)); \
                break;                                                                      \
            }

            const auto fundamental_type = reinterpret_cast<const FundamentalType*>(type);

            switch (fundamental_type->fundamental_kind)
            {
                /*
                 * Booleans have to be treated special so the serializer doesn't end up with a value outside
                 * the 0 - 1 range when casting from u8 (which has a valid range of 0-255)
                 */
                case FundamentalKind::bool_kind:
                {
                    if (serializer->mode == SerializerMode::writing)
                    {
                        // Casting before serializing removes any values outside the zero-one range
                        bool value = !!*reinterpret_cast<bool*>(dst); // NOLINT
                        serializer->serialize_fundamental(&value);
                    }
                    else
                    {
                        serializer->serialize_fundamental(reinterpret_cast<bool*>(dst));
                    }
                    break;
                }
                BEE_SERIALIZE_FUNDAMENTAL(char_kind, char)
                BEE_SERIALIZE_FUNDAMENTAL(signed_char_kind, i8)
                BEE_SERIALIZE_FUNDAMENTAL(unsigned_char_kind, u8)
                BEE_SERIALIZE_FUNDAMENTAL(short_kind, i16)
                BEE_SERIALIZE_FUNDAMENTAL(unsigned_short_kind, u16)
                BEE_SERIALIZE_FUNDAMENTAL(int_kind, i32)
                BEE_SERIALIZE_FUNDAMENTAL(unsigned_int_kind, u32)
                BEE_SERIALIZE_FUNDAMENTAL(long_kind, i32)
                BEE_SERIALIZE_FUNDAMENTAL(unsigned_long_kind, u32)
                BEE_SERIALIZE_FUNDAMENTAL(long_long_kind, i64)
                BEE_SERIALIZE_FUNDAMENTAL(unsigned_long_long_kind, u64)
                BEE_SERIALIZE_FUNDAMENTAL(float_kind, float)
                BEE_SERIALIZE_FUNDAMENTAL(double_kind, double)
                default: break;
            }

#undef BEE_SERIALIZE_FUNDAMENTAL
        }
        default: break;
    }
}


template <typename SerializerType, typename DataType>
inline void serialize(const SerializerMode mode, SerializerType* serializer, DataType* dst)
{
    static_assert(std::is_base_of_v<Serializer, SerializerType>, "SerializerType must be a subclass of bee::Serializer");

    const auto type = get_type<DataType>();
    if (BEE_FAIL_F(type->kind != TypeKind::unknown, "`DataType` is not marked for reflection - use BEE_REFLECT() on the types declaration"))
    {
        return;
    }

    serializer->mode = mode;
    serializer->version = 0;

    if (BEE_FAIL_F(serializer->begin(), "Failed to initialize serialization"))
    {
        return;
    }

    serialize_type(serializer, type, reinterpret_cast<u8*>(dst));
    serializer->end();
}


} // namespace bee