/*
 *  Serialization.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Serialization/Serialization.hpp"


namespace bee {


void serialize_version(Serializer* serializer, i32* version);


Serializer::Serializer(const bee::SerializerFormat serialized_format)
    : format(serialized_format)
{}


SerializationBuilder::SerializationBuilder(Serializer* new_serializer, const RecordType* type)
    : serializer_(new_serializer),
      type_(type)
{}

SerializationBuilder::~SerializationBuilder()
{
    if (container_kind_ == SerializedContainerKind::none || container_kind_ == SerializedContainerKind::text)
    {
        return;
    }

    if (container_kind_ == SerializedContainerKind::key_value)
    {
        serializer_->end_object();
    }
    else
    {
        serializer_->end_array();
    }
}

SerializationBuilder& SerializationBuilder::structure(const i32 serialized_version)
{
    if (BEE_FAIL_F(version_ <= 0, "serialized version has already been set"))
    {
        return *this;
    }

    version_ = serialized_version;
    serialize_version(serializer_, &version_);
    return *this;
}

SerializationBuilder& SerializationBuilder::container(const SerializedContainerKind kind, i32* size)
{
    if (BEE_FAIL_F(version_ <= 0, "serialized version has already been set"))
    {
        return *this;
    }

    version_ = 1;
    container_kind_ = kind;

    switch (container_kind_)
    {
        case SerializedContainerKind::sequential:
        {
            serializer_->begin_array(size);
            break;
        }
        case SerializedContainerKind::key_value:
        {
            serializer_->begin_object(size);
            break;
        }
        case SerializedContainerKind::text:
        {
            serializer_->begin_text(size);
            break;
        }
        default:
        {
            BEE_UNREACHABLE("Invalid container type");
        }
    }

    return *this;
}

SerializationBuilder& SerializationBuilder::text(char* buffer, const i32 size, const i32 capacity)
{
    if (BEE_FAIL_F(container_kind_ == SerializedContainerKind::text, "serialization builder is not configured to serialize a text container"))
    {
        return *this;
    }

    serializer_->end_text(buffer, size, capacity);
    return *this;
}

SerializationBuilder& SerializationBuilder::key(String* data)
{
    BEE_ASSERT_F(container_kind_ == SerializedContainerKind::key_value, "serialization builder is not configured to build a container type");
    serializer_->serialize_key(data);
    return *this;
}


void serialize_version(Serializer* serializer, i32* version)
{
    serializer->serialize_field("bee::version");
    serializer->serialize_fundamental(version);
}

void serialize_serialization_flags(Serializer* serializer, SerializationFlags* flags)
{
    serializer->serialize_field("bee::flags");

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
            serializer->serialize_field(field.name);

            if (field.template_argument_in_parent >= 0)
            {
                serialized_type = template_args[field.template_argument_in_parent];
            }

            serialize_type(serializer, serialized_type, field.serializer_function, data + field.offset);
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
                serializer->serialize_field(field.name);
                if (field.template_argument_in_parent < 0)
                {
                    serialize_type(serializer, field.type, field.serializer_function, data + field.offset);
                }
                else
                {
                    serialize_type(serializer, template_args[field.template_argument_in_parent], field.serializer_function, data + field.offset);
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
                serializer->serialize_field(field.name);

                if (field.template_argument_in_parent < 0)
                {
                    serialize_type(serializer, field.type, field.serializer_function, data + field.offset);
                }
                else
                {
                    serialize_type(serializer, template_args[field.template_argument_in_parent], field.serializer_function, data + field.offset);
                }
            }
        }
    }
}

BEE_CORE_API void serialize_type(Serializer* serializer, const Type* type, SerializationFunction* serialization_function, u8* data, const Span<const Type*>& template_type_arguments)
{
    if (type->serialized_version <= 0)
    {
        log_error("Skipping serialization for `%s`: type is not marked for serialization using the `serializable` attribute", type->name);
        return;
    }

    // Handle custom serialization
    if (serialization_function != nullptr)
    {
        BEE_ASSERT_F((type->kind & TypeKind::record) != TypeKind::unknown, "Custom serializer functions must only be used with record types");

        SerializationBuilder builder(serializer, type->as<RecordType>());
        serialization_function->serialize(&builder, data);
        return;
    }

    // Handle as automatically serialized
    if (type->is(TypeKind::record))
    {
        auto record_type = type->as<RecordType>();
        auto serialization_flags = type->serialization_flags;

        serializer->begin_record(record_type);

        i32 version = type->serialized_version;
        serialize_version(serializer, &version);

        serialize_serialization_flags(serializer, &serialization_flags);

        if (serializer->format == SerializerFormat::text || (serialization_flags & SerializationFlags::packed_format) == SerializationFlags::packed_format)
        {
            BEE_ASSERT_F(version <= type->serialized_version, "serialization error for type `%s`: structures serialized using `packed_format` are not forward-compatible with versions from the future", type->name);
            serialize_packed_record(version, serializer, record_type, data, template_type_arguments);
        }

        if ((serialization_flags & SerializationFlags::table_format) == SerializationFlags::table_format)
        {
            serialize_table_record(version, serializer, record_type, data, template_type_arguments);
        }

        serializer->end_record();
    }
    else if (type->is(TypeKind::array))
    {
        auto array_type = type->as<ArrayType>();
        auto element_type = array_type->element_type;

        i32 element_count = array_type->element_count;
        serializer->begin_array(&element_count);

        for (int element = 0; element < element_count; ++element)
        {
            serialize_type(serializer, element_type, nullptr, data + element_type->size * element);
        }

        serializer->end_array();
    }
    else if (type->is(TypeKind::fundamental))
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

BEE_CORE_API void serialize_type(Serializer* serializer, const Type* type, SerializationFunction* serialization_function, u8* data)
{
    return serialize_type(serializer, type, serialization_function, data, {});
}


} // namespace bee