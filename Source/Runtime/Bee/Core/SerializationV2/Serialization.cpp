/*
 *  Serialization.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/SerializationV2/Serialization.hpp"


namespace bee {


void serialize_version(Serializer* serializer, i32* version);


Serializer::Serializer(const bee::SerializerFormat serialized_format)
    : format(serialized_format)
{}


SerializationBuilder::SerializationBuilder(bee::Serializer* new_serializer, const bee::Type* type, bee::u8* data)
    : version_(1),
      serializer_(new_serializer),
      serialized_type_(type),
      serialized_data_(data)
{}

SerializationBuilder& SerializationBuilder::version(const i32 value)
{
    version_ = value;
    serialize_version(serializer_, &version_);
    BEE_ASSERT_F(version_ <= value, "serialization error for type `%s`: SerializationBuilder functions are not forward-compatible with versions from the future", serialized_type_->name);
    return *this;
}


void serialize_version(Serializer* serializer, i32* version)
{
    static Field version_field(get_type_hash("bee::version"), 0, Qualifier::none, StorageClass::none, "bee::version", get_type<i32>(), {}, {}, 1);
    serializer->serialize_field(version_field);
    serializer->serialize_fundamental(version);
}

void serialize_element_count(Serializer* serializer, i32* count)
{
    static Field element_count_field(get_type_hash("bee::element_count"), 0, Qualifier::none, StorageClass::none, "bee::element_count", get_type<i32>(), {}, {}, 1);
    serializer->serialize_field(element_count_field);
    serializer->serialize_fundamental(count);
}

void serialize_serialization_flags(Serializer* serializer, SerializationFlags* flags)
{
    static Field flags_field(get_type_hash("bee::flags"), 0, Qualifier::none, StorageClass::none, "bee::flags", get_type<std::underlying_type_t<SerializationFlags>>(), {}, {}, 1);

    serializer->serialize_field(flags_field);

    auto integral = underlying_flag_t(*flags);
    serializer->serialize_fundamental(&integral);

    if (serializer->mode == SerializerMode::reading)
    {
        *flags = static_cast<SerializationFlags>(integral);
    }
}


void serialize_packed_record(const i32 version, Serializer* serializer, const RecordType* type, u8* data, const Span<const Type*> template_args)
{
    for (const Field& field : type->fields)
    {
        auto serialized_type = field.type;

        if (field.version_added > 0 && version >= field.version_added && version < field.version_removed)
        {
            serializer->serialize_field(field);

            if (field.template_argument_in_parent >= 0)
            {
                serialized_type = template_args[field.template_argument_in_parent];
            }

            serialize_type(serialized_type->serialized_version, serializer, serialized_type, data + field.offset);
        }
    }
}

void serialize_table_record(const i32 version, Serializer* serializer, const RecordType* type, u8* data, const Span<const Type*> template_args)
{
    int field_count = type->fields.size();

    // Exclude any nonserialized or older/newer fields when writing
    if (serializer->mode == SerializerMode::writing)
    {
        for (const Field& field : type->fields)
        {
            if (field.version_added <= 0 || field.version_removed <= version)
            {
                --field_count;
            }
        }
    }

    serializer->serialize_fundamental(&field_count);

    if (serializer->mode == SerializerMode::reading)
    {
        for (int f = 0; f < field_count; ++f)
        {
            FieldHeader header{};
            serializer->serialize_bytes(&header, sizeof(FieldHeader));

            // Lookup the type using the header hashes
            const auto field_index = container_index_of(type->fields, [&](const Field& f)
            {
                return f.type->hash == header.type_hash && f.hash == header.field_hash;
            });

            if (BEE_FAIL_F(field_index >= 0, "serialization of record type `%s` failed: detected missing field. The fields may have been renamed or it's type changed", type->name))
            {
                return;
            }

            auto& field = type->fields[field_index];
            if (field.version_added > 0 && version >= field.version_added && version < field.version_removed)
            {
                serializer->serialize_field(field);
                if (field.template_argument_in_parent < 0)
                {
                    serialize_type(field.type->serialized_version, serializer, field.type, data + field.offset);
                }
                else
                {
                    serialize_type(template_args[field.template_argument_in_parent]->serialized_version, serializer, template_args[field.template_argument_in_parent], data + field.offset);
                }
            }
        }
    }
    else
    {
        // We have to iterate all the fields to skip the ones we don't want to write
        for (const Field& field : type->fields)
        {
            if (field.version_added > 0 && version >= field.version_added && version < field.version_removed)
            {
                FieldHeader header(field);
                serializer->serialize_bytes(&header, sizeof(FieldHeader));
                serializer->serialize_field(field);

                if (field.template_argument_in_parent < 0)
                {
                    serialize_type(field.type->serialized_version, serializer, field.type, data + field.offset);
                }
                else
                {
                    serialize_type(template_args[field.template_argument_in_parent]->serialized_version, serializer, template_args[field.template_argument_in_parent], data + field.offset);
                }
            }
        }
    }
}

BEE_CORE_API void serialize_type(const i32 serialized_version, Serializer* serializer, const Type* type, u8* data, const Span<const Type*>& template_type_arguments)
{
    if (type->serialized_version <= 0)
    {
        log_error("Skipping serialization for `%s`: type is not marked for serialization using the `serializable` attribute", type->name);
        return;
    }

    // Handle custom serialization
    if ((type->serialization_flags & SerializationFlags::uses_builder) == SerializationFlags::uses_builder)
    {
        BEE_ASSERT_F((type->kind & TypeKind::record) != TypeKind::unknown, "Custom serializer functions must only be used with record types");

        const auto record_type = type->as<RecordType>();

        BEE_ASSERT_F(record_type->serializer_function != nullptr, "Missing serializer function for type %s", type->name);

        SerializationBuilder builder(serializer, type, data);
        record_type->serializer_function(&builder);

        return;
    }

    // Handle as automatically serialized
    if (type->is<TypeKind::record>())
    {
        auto record_type = type->as<RecordType>();
        auto serialization_flags = type->serialization_flags;

        serializer->begin_record(record_type);

        i32 version = serialized_version;
        serialize_version(serializer, &version);

        serialize_serialization_flags(serializer, &serialization_flags);

        if (serializer->format == SerializerFormat::text || (serialization_flags & SerializationFlags::packed_format) == SerializationFlags::packed_format)
        {
            BEE_ASSERT_F(version <= serialized_version, "serialization error for type `%s`: structures serialized using `packed_format` are not forward-compatible with versions from the future", type->name);
            serialize_packed_record(version, serializer, record_type, data, template_type_arguments);
        }

        if ((serialization_flags & SerializationFlags::table_format) == SerializationFlags::table_format)
        {
            serialize_table_record(version, serializer, record_type, data, template_type_arguments);
        }

        serializer->end_record();
    }

    if (type->is<TypeKind::array>())
    {
        auto array_type = type->as<ArrayType>();
        auto element_type = array_type->element_type;

        i32 element_count = array_type->element_count;
        serializer->begin_array(&element_count);

        for (int element = 0; element < element_count; ++element)
        {
            serialize_type(element_type->serialized_version, serializer, element_type, data + element_type->size * element);
        }

        serializer->end_array();
    }

    if (type->is<TypeKind::fundamental>())
    {
        const auto fundamental_type = type->as<FundamentalType>();

#define BEE_SERIALIZE_FUNDAMENTAL(kind_name, serialized_type) case FundamentalKind::kind_name: { serializer->serialize_fundamental(reinterpret_cast<serialized_type*>(data)); break; }
        switch (fundamental_type->fundamental_kind)
        {
            case FundamentalKind::bool_kind:
            {
                bool value = *reinterpret_cast<bool*>(data) ? true : false;
                serializer->serialize_fundamental(&value);
                if (serializer->mode == SerializerMode::reading)
                {
                    memcpy(data, &value, sizeof(bool));
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
}

BEE_CORE_API void serialize_type(const i32 serialized_version, Serializer* serializer, const Type* type, u8* data)
{
    return serialize_type(serialized_version, serializer, type, data, {});
}


} // namespace bee