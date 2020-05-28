/*
 *  Serialization.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Serialization/Serialization.hpp"
#include "Bee/Core/IO.hpp"

#include <inttypes.h>


namespace bee {


void serialize_version(Serializer* serializer, i32* version);


Serializer::Serializer(const bee::SerializerFormat serialized_format)
    : format(serialized_format)
{}


SerializationBuilder::SerializationBuilder(Serializer* new_serializer, const RecordType* type, Allocator* allocator)
    : serializer_(new_serializer),
      type_(type),
      allocator_(allocator)
{}

SerializationBuilder::~SerializationBuilder()
{
    switch (container_kind_)
    {
        case SerializedContainerKind::none:
        {
            serializer_->end_record();
            break;
        }
        case SerializedContainerKind::sequential:
        {
            serializer_->end_array();
            break;
        }
        case SerializedContainerKind::key_value:
        {
            serializer_->end_object();
            break;
        }
        default: break;
    }
}

SerializationBuilder& SerializationBuilder::structure(const i32 serialized_version)
{
    if (BEE_FAIL_F(version_ <= 0, "serialized version has already been set"))
    {
        return *this;
    }

    serializer_->begin_record(type_);
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
        const auto* serialized_type = field.type;
        BEE_ASSERT(serialized_type != nullptr);

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
            const auto field_index = find_index_if(type->fields, [&](const Field& f)
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

enum class SerializeTypeMode
{
    new_scope,
    append_scope
};

void serialize_type(const SerializeTypeMode serialize_type_mode, Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data, const Span<const Type*>& template_type_arguments, Allocator* builder_allocator)
{
    static thread_local char enum_constant_buffer[1024];

    if (type->serialized_version <= 0)
    {
        log_error("Skipping serialization for `%s`: type is not marked for serialization using the `serializable` attribute", type->name);
        return;
    }

    // Handle custom serialization
    if (serialization_function != nullptr)
    {
        BEE_ASSERT_F((type->kind & TypeKind::record) != TypeKind::unknown, "Custom serializer functions must only be used with record types");

        SerializationBuilder builder(serializer, type->as<RecordType>(), builder_allocator);
        serialization_function(&builder, data);
        return;
    }

    // Handle as automatically serialized
    if (type->is(TypeKind::record))
    {
        auto record_type = type->as<RecordType>();
        for (const Type* base_type : record_type->base_records)
        {
            // FIXME(Jacob): is this the best way to handle inheritence?
            // FIXME(Jacob): base types need their serialization builder functions because they're not fields
            serialize_type(serialize_type_mode, serializer, base_type, nullptr, data, template_type_arguments, builder_allocator);
        }

        if (serialize_type_mode != SerializeTypeMode::append_scope)
        {
            serializer->begin_record(record_type);
        }

        // Serialize the flags and version numbers of the type if we're not dealing with relaxed sources
        auto serialization_flags = type->serialization_flags;
        i32 version = type->serialized_version;

        if ((serializer->source_flags & SerializerSourceFlags::unversioned) == SerializerSourceFlags::none)
        {
            serialize_version(serializer, &version);
        }

        if ((serializer->source_flags & SerializerSourceFlags::dont_serialize_flags) == SerializerSourceFlags::none)
        {
            serialize_serialization_flags(serializer, &serialization_flags);
        }

        if (serializer->format == SerializerFormat::text || (serialization_flags & SerializationFlags::packed_format) == SerializationFlags::packed_format)
        {
            BEE_ASSERT_F(version <= type->serialized_version, "serialization error for type `%s`: structures serialized using `packed_format` are not forward-compatible with versions from the future", type->name);
            serialize_packed_record(version, serializer, record_type, data, template_type_arguments);
        }

        if ((serialization_flags & SerializationFlags::table_format) == SerializationFlags::table_format)
        {
            serialize_table_record(version, serializer, record_type, data, template_type_arguments);
        }

        if (serialize_type_mode != SerializeTypeMode::append_scope)
        {
            serializer->end_record();
        }
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
    else if (type->is(TypeKind::enum_decl))
    {
        auto as_enum = type->as<EnumType>();
        // Binary doesn't need special treatment - just serialize as underlying fundamental
        if (serializer->format == SerializerFormat::binary)
        {
            serialize_type(serializer, as_enum->underlying_type, nullptr, data);
        }
        else
        {
            // Text format needs to either display the constant name or a series of flags. If the value isn't a valid
            // power of two then it should be parsed as an integer
            if (!as_enum->is_flags)
            {
                if (serializer->mode == SerializerMode::writing)
                {
                    i64 value = 0;
                    memcpy(&value, data, as_enum->underlying_type->size);

                    const auto constant_index = find_index_if(as_enum->constants, [&](const EnumConstant& c)
                    {
                        return c.value == value;
                    });

                    if (constant_index < 0)
                    {
                        str::format_buffer(enum_constant_buffer, static_array_length(enum_constant_buffer), "%" PRId64, value);
                    }
                    else
                    {
                        const char* constant_name = as_enum->constants[constant_index].name;
                        str::format_buffer(enum_constant_buffer, static_array_length(enum_constant_buffer), "%s", constant_name);
                    }

                    int size = str::length(enum_constant_buffer);
                    serializer->begin_text(&size);
                    serializer->end_text(enum_constant_buffer, size, static_array_length(enum_constant_buffer));
                }
                else
                {
                    int size = 0;
                    serializer->begin_text(&size);

                    BEE_ASSERT(size <= static_array_length(enum_constant_buffer));

                    serializer->end_text(enum_constant_buffer, size, static_array_length(enum_constant_buffer));
                    const auto constant_hash = get_type_hash({ enum_constant_buffer, size });

                    const auto constant_index = find_index_if(as_enum->constants, [&](const EnumConstant& c)
                    {
                        return c.hash == constant_hash;
                    });

                    // Invalid enum constant - just parse as int
                    if (constant_index < 0)
                    {
                        i64 value = 0;
                        sscanf(enum_constant_buffer, "%" SCNd64, &value);
                        memcpy(data, &value, as_enum->underlying_type->size);
                    }
                    else
                    {
                        // valid so we can copy the constant value into data
                        memcpy(data, &as_enum->constants[constant_index].value, as_enum->underlying_type->size);
                    }
                }
            }
            else
            {
                i64 value = 0;
                memcpy(&value, data, as_enum->underlying_type->size);

                if (serializer->mode == SerializerMode::writing)
                {
                    io::StringStream stream(enum_constant_buffer, static_array_length(enum_constant_buffer), 0);

                    for (const auto constant : enumerate(as_enum->constants))
                    {
                        if ((value & constant.value.value) != 0)
                        {
                            stream.write_fmt("%s", constant.value.name);

                            if (constant.index < as_enum->constants.size() - 1)
                            {
                                stream.write(" | ");
                            }
                        }
                    }

                    int size = stream.size();
                    serializer->begin_text(&size);
                    serializer->end_text(enum_constant_buffer, size, stream.capacity());
                }
                else
                {
                    int size = 0;
                    serializer->begin_text(&size);

                    BEE_ASSERT(size <= static_array_length(enum_constant_buffer));

                    serializer->end_text(enum_constant_buffer, size, static_array_length(enum_constant_buffer));

                    io::StringStream stream(enum_constant_buffer, static_array_length(enum_constant_buffer), 0);

                    const char* flag_begin = enum_constant_buffer;
                    const char* flag_end = flag_begin;
                    i64 flag_as_int = 0;
                    i64 final_flag = 0;

                    for (int offset = 0; offset < size; ++offset)
                    {
                        if (!str::is_space(enum_constant_buffer[offset]) && enum_constant_buffer[offset] != '|')
                        {
                            ++flag_end;
                            continue;
                        }

                        const auto flag_hash = get_type_hash(StringView(flag_begin, sign_cast<i32>(flag_end - flag_begin)));
                        const auto flag_index = find_index_if(as_enum->constants, [&](const EnumConstant& c)
                        {
                            return c.hash == flag_hash;
                        });

                        if (flag_index < 0)
                        {
                            sscanf(enum_constant_buffer, "%" SCNd64, &flag_as_int);
                        }
                        else
                        {
                            flag_as_int = as_enum->constants[flag_index].value;
                        }

                        final_flag |= flag_as_int;
                    }

                    memcpy(data, &final_flag, as_enum->underlying_type->size);
                }
            }
        }
    }
    else if (type->is(TypeKind::fundamental))
    {
        const auto fundamental_type = type->as<FundamentalType>();

#define BEE_SERIALIZE_FUNDAMENTAL(kind_name, serialized_type) case FundamentalKind::kind_name: { serializer->serialize_fundamental(reinterpret_cast<serialized_type*>(data)); break; }
        switch (fundamental_type->fundamental_kind)
        {
            case FundamentalKind::bool_kind:
            {
                bool value = *reinterpret_cast<bool*>(data) ? true : false; // NOLINT
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
            BEE_SERIALIZE_FUNDAMENTAL(u128_kind, u128)
            default: break;
        }
#undef BEE_SERIALIZE_FUNDAMENTAL
    }
}

void serialize_type(Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data, Allocator* builder_allocator)
{
    return serialize_type(SerializeTypeMode::new_scope, serializer, type, serialization_function, data, {}, builder_allocator);
}

void serialize_type(Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data, const Span<const Type*>& template_type_arguments, Allocator* builder_allocator)
{
    return serialize_type(SerializeTypeMode::new_scope, serializer, type, serialization_function, data, template_type_arguments, builder_allocator);
}

void serialize_type_append(Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data, Allocator* builder_allocator)
{
    serialize_type(SerializeTypeMode::append_scope, serializer, type, serialization_function, data, {}, builder_allocator);
}

void serialize_type_append(Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data, const Span<const Type*>& template_type_arguments, Allocator* builder_allocator)
{
    serialize_type(SerializeTypeMode::append_scope, serializer, type, serialization_function, data, template_type_arguments, builder_allocator);
}


} // namespace bee