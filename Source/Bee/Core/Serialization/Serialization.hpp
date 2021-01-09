/*
 *  Serialization.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Containers/StaticArray.hpp"


#define BEE_SERIALIZE_TYPE inline void custom_serialize_type


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

enum class SerializedContainerKind
{
    none,
    structure,
    sequential,
    key_value,
    text,
    bytes
};

BEE_FLAGS(SerializerSourceFlags, u32)
{
    none                    = 0u,
    unversioned             = 1u << 0u,
    dont_serialize_flags    = 1u << 1u,
    all                     = unversioned | dont_serialize_flags
};


struct BEE_CORE_API Serializer
{
    SerializerMode          mode { SerializerMode::reading };
    const SerializerFormat  format {SerializerFormat::unknown };
    SerializerSourceFlags   source_flags { SerializerSourceFlags::none };

    explicit Serializer(const SerializerFormat serialized_format);

    virtual size_t offset() = 0;
    virtual size_t capacity() = 0;

    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual void begin_record(const RecordType& type) = 0;
    virtual void end_record() = 0;
    virtual void begin_object(i32* member_count) = 0;
    virtual void end_object() = 0;
    virtual void begin_array(i32* count) = 0;
    virtual void end_array() = 0;
    virtual void begin_text(i32* length) = 0;
    virtual void end_text(char* buffer, const i32 size, const i32 capacity) = 0;
    virtual void begin_bytes(i32* size) = 0;
    virtual void end_bytes(u8* buffer, const i32 size) = 0;
    virtual bool serialize_field(const char* name) = 0;
    virtual void serialize_key(String* key) = 0;
    virtual void serialize_fundamental(bool* data) = 0;
    virtual void serialize_fundamental(char* data) = 0;
    virtual void serialize_fundamental(float* data) = 0;
    virtual void serialize_fundamental(double* data) = 0;
    virtual void serialize_fundamental(u8* data) = 0;
    virtual void serialize_fundamental(u16* data) = 0;
    virtual void serialize_fundamental(u32* data) = 0;
    virtual void serialize_fundamental(u64* data) = 0;
    virtual void serialize_fundamental(i8* data) = 0;
    virtual void serialize_fundamental(i16* data) = 0;
    virtual void serialize_fundamental(i32* data) = 0;
    virtual void serialize_fundamental(i64* data) = 0;
    virtual void serialize_fundamental(u128* data) = 0;
};

struct SerializeTypeParams final : public Noncopyable
{
    Type                            type;
    u8*                             data { nullptr };
    Allocator*                      builder_allocator { system_allocator() };
    Field::serialization_function_t serialization_function { nullptr };
    Span<const Type>                template_type_arguments;
    SerializationFlags              field_flags { SerializationFlags::none };

    SerializeTypeParams(
        const Type&                     new_type,
        u8*                             new_data,
        Allocator*                      new_builder_allocator,
        Field::serialization_function_t new_serialization_function,
        const SerializationFlags        new_field_flags
    ) : type(new_type),
        data(new_data),
        builder_allocator(new_builder_allocator),
        serialization_function(new_serialization_function),
        field_flags(new_field_flags)
    {}

    SerializeTypeParams(
        const Type&                     new_type,
        u8*                             new_data,
        Allocator*                      new_builder_allocator,
        Field::serialization_function_t new_serialization_function,
        const Span<const Type>&         new_template_type_args,
        const SerializationFlags        new_field_flags
    ) : type(new_type),
        data(new_data),
        builder_allocator(new_builder_allocator),
        serialization_function(new_serialization_function),
        template_type_arguments(new_template_type_args),
        field_flags(new_field_flags)
    {}

    SerializationFlags merged_flags() const
    {
        return field_flags | type->serialization_flags;
    }
};

BEE_CORE_API void serialize_type(Serializer* serializer, const SerializeTypeParams& params);

BEE_CORE_API void serialize_type_append(Serializer* serializer, const SerializeTypeParams& params);


template <typename T>
void custom_serialize_type(bee::SerializationBuilder* builder, T* data) {}

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
    SerializationBuilder(Serializer* new_serializer, const SerializeTypeParams* params);

    ~SerializationBuilder();

    template <typename FieldType>
    inline SerializationBuilder& add_field(const i32 version_added, FieldType* field, const char* field_name)
    {
        return add_field(version_added, limits::max<i32>(), field, field_name);
    }

    template <typename FieldType>
    inline SerializationBuilder& add_field(const i32 version_added, const i32 version_removed, FieldType* field, const char* field_name)
    {
        BEE_ASSERT_F(container_kind_ == SerializedContainerKind::none, "serialization builder is not configured to build a structure - cannot add fields to non-structure types");

        const auto field_type = get_type<FieldType>();

        if (version_ < version_added || version_ >= version_removed)
        {
            return *this;
        }

        serializer_->serialize_field(field_name);

        SerializeTypeParams field_params(
            field_type,
            reinterpret_cast<u8*>(field),
            allocator(),
            nullptr,
            SerializationFlags::none
        );

        if ((field_type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
        {
            SerializationBuilder builder(serializer_, &field_params);
            custom_serialize_type(&builder, field);
        }
        else
        {
            serialize_type(serializer_, field_params);
        }
        return *this;
    }

    template <typename FieldType>
    inline SerializationBuilder& remove_field(const i32 version_added, const i32 version_removed, const FieldType& default_value, const char* field_name)
    {
        BEE_ASSERT_F(container_kind_ == SerializedContainerKind::none, "serialization builder is not configured to build a structure - cannot remove fields from non-structure types");

        const auto field_type = get_type<FieldType>();
        if (version_ < version_added || version_ >= version_removed)
        {
            return *this;
        }

        FieldType removed_data;

        if (serializer_->mode == SerializerMode::writing)
        {
            copy(&removed_data, &default_value, 1);
        }

        serializer_->serialize_field(field_name);

        SerializeTypeParams field_params(
            field_type,
            reinterpret_cast<u8*>(&removed_data),
            allocator(),
            nullptr,
            SerializationFlags::none
        );

        if ((field_type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
        {
            SerializationBuilder builder(serializer_, &field_params);
            custom_serialize_type(&builder, &removed_data);
        }
        else
        {
            serialize_type(serializer_, field_params);
        }

        return *this;
    }

    SerializationBuilder& structure(const i32 serialized_version);

    SerializationBuilder& container(const SerializedContainerKind kind, i32* size);

    SerializationBuilder& text(char* buffer, const i32 size, const i32 capacity);

    SerializationBuilder& bytes(u8* buffer, const i32 size);

    SerializationBuilder& key(String* data);

    template <typename T>
    inline SerializationBuilder& element(T* data)
    {
        BEE_ASSERT_F(container_kind_ != SerializedContainerKind::none, "serialization builder is not configured to build a container type");

        auto type = get_type<T>();

        SerializeTypeParams params(
            type,
            reinterpret_cast<u8*>(data),
            allocator(),
            nullptr,
            SerializationFlags::none
        );
        if ((type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
        {
            SerializationBuilder builder(serializer_, &params);
            custom_serialize_type(&builder, data);
        }
        else
        {
            serialize_type(serializer_, params);
        }

        return *this;
    }

    inline SerializerMode mode() const
    {
        return serializer_->mode;
    }

    inline SerializerFormat format() const
    {
        return serializer_->format;
    }

    inline Serializer* serializer()
    {
        return serializer_;
    }

    inline Allocator* allocator()
    {
        return params_->builder_allocator;
    }

    inline const RecordType& type() const
    {
        return type_;
    }

    inline const SerializeTypeParams& params() const
    {
        return *params_;
    }

private:
    Serializer*                 serializer_ { nullptr };
    const SerializeTypeParams*  params_ { nullptr };
    RecordType                  type_ { nullptr };
    SerializationFlags          field_flags_ { SerializationFlags::none };
    SerializedContainerKind     container_kind_ { SerializedContainerKind::none };
    i32                         version_ { -1 };
};


template <typename DataType, typename ReflectedType = DataType>
inline void serialize(const SerializerMode mode, Serializer* serializer, DataType* data, Allocator* builder_allocator = system_allocator())
{
    BEE_ASSERT_F(serializer->format != SerializerFormat::unknown, "Serializer has an invalid kind");

    const auto type = get_type<ReflectedType>();
    if (BEE_FAIL_F(type->kind != TypeKind::unknown, "`DataType` is not marked for reflection - use BEE_REFLECT() on the types declaration"))
    {
        return;
    }

    serializer->mode = mode;

    if (BEE_FAIL_F(serializer->begin(), "Failed to initialize serialization"))
    {
        return;
    }

    SerializeTypeParams params(
        type,
        reinterpret_cast<u8*>(data),
        builder_allocator,
        nullptr,
        SerializationFlags::none
    );

    if ((type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
    {
        SerializationBuilder builder(serializer, &params);
        custom_serialize_type(&builder, data);
    }
    else
    {
        serialize_type(serializer, params);
    }

    serializer->end();
}

template <typename DataType, typename ReflectedType = DataType>
inline void serialize(const SerializerMode mode, const SerializerSourceFlags source_flags, Serializer* serializer, DataType* data, Allocator* builder_allocator = system_allocator())
{
    serializer->source_flags = source_flags;
    serialize(mode, serializer, data, builder_allocator);
}

/*
 ******************************
 *
 * Type serialization
 *
 ******************************
 */
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, Type* type)
{
    u32 hash = (*type)->hash;
    builder->structure(1).add_field(1, &hash, "hash");
    if (builder->mode() == SerializerMode::reading)
    {
        *type = get_type(hash);
    }
}

/*
 ******************************
 *
 * TypeInstance serialization
 *
 ******************************
 */
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, TypeInstance* instance)
{
    builder->serializer()->begin_record(get_type_as<TypeInstance, RecordTypeInfo>());
    {

        builder->serializer()->serialize_field("bee::type");
        u32 type_hash = instance->is_valid() ? instance->type()->hash : get_type<UnknownTypeInfo>()->hash;
        builder->serializer()->serialize_fundamental(&type_hash);

        if (builder->mode() == SerializerMode::reading)
        {
            const auto type = get_type(type_hash);

            if (type->is(TypeKind::unknown))
            {
                return;
            }

            auto* allocator = instance->allocator() != nullptr ? instance->allocator() : builder->allocator();
            *instance = BEE_MOVE(type->create_instance(allocator));
        }

        if (builder->mode() == SerializerMode::reading || instance->is_valid())
        {
            BEE_ASSERT(instance->data() != nullptr);

            auto* data = static_cast<u8*>(const_cast<void*>(instance->data()));
            SerializeTypeParams params(
                instance->type(),
                data,
                builder->allocator(),
                nullptr,
                {},
                builder->params().field_flags
            );
            serialize_type_append(builder->serializer(), params);
        }

    }
    builder->serializer()->end_record();
}


/*
 **********************
 *
 * Array serialization
 *
 **********************
 */
template <typename T, ContainerMode Mode>
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, Array<T, Mode>* array)
{
    const Type stored_type = get_type<T>();
    auto container_kind = SerializedContainerKind::sequential;
    if ((builder->params().merged_flags() & SerializationFlags::bytes) != SerializationFlags::none)
    {
        container_kind = SerializedContainerKind::bytes;
    }

    const bool reading = builder->mode() == SerializerMode::reading;

    int size = array->size();
    if (container_kind == SerializedContainerKind::bytes)
    {
        size = sizeof(T) * array->size();
    }

    builder->container(container_kind, &size);

    if (reading)
    {
        array->resize(size);
    }

    if (container_kind == SerializedContainerKind::bytes)
    {
        builder->bytes(reinterpret_cast<u8*>(array->data()), size);
    }
    else
    {
        for (auto& element : *array)
        {
            builder->element(&element);
        }
    }
}


/*
 **************************
 *
 * HashMap serialization
 *
 **************************
 */
template <
    typename        KeyType,
    typename        ValueType,
    ContainerMode   Mode,
    typename        Hasher,
    typename        KeyEqual
>
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>* map)
{
    int size = map->size();
    builder->container(SerializedContainerKind::key_value, &size);

    if (builder->mode() == SerializerMode::reading)
    {
        KeyType key;

        for (int i = 0; i < size; ++i)
        {
            ValueType value;
            builder->key(&key);
            builder->element(&value);
            map->insert(key, BEE_MOVE(value));
        }
    }
    else
    {
        for (KeyValuePair<KeyType, ValueType>& elem : *map)
        {
            builder->key(&elem.key);
            builder->element(&elem.value);
        }
    }
}

/*
 **********************
 *
 * String serialization
 *
 **********************
 */
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, String* string)
{
    int size = string->size();
    builder->container(SerializedContainerKind::text, &size);

    if (builder->mode() == SerializerMode::reading)
    {
        string->resize(size);
    }

    builder->text(string->data(), string->size(), string->capacity());
}

/*
 ******************************
 *
 * StaticString serialization
 *
 ******************************
 */
template <i32 Size>
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, StaticString<Size>* string)
{
    int size = string->size();
    builder->container(SerializedContainerKind::text, &size);

    if (builder->mode() == SerializerMode::reading)
    {
        string->resize(size);
    }

    builder->text(string->data(), string->size(), string->capacity());
}


/*
 **********************
 *
 * Path serialization
 *
 **********************
 */
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, Path* path)
{
    int size = path->size();
    builder->container(SerializedContainerKind::text, &size);

    if (builder->mode() == SerializerMode::reading)
    {
        path->data_.resize(size);
    }

    builder->text(path->data_.data(), path->data_.size(), path->data_.capacity());
}


/*
 ************************
 *
 * Buffer serialization
 *
 ************************
 */
template <typename T, i32 Capacity, typename SizeType>
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, StaticArray<T, Capacity, SizeType>* buffer)
{
    auto container_kind = SerializedContainerKind::sequential;

    if ((builder->params().merged_flags() & SerializationFlags::bytes) != SerializationFlags::none)
    {
        container_kind = SerializedContainerKind::bytes;
    }

    int size = buffer->size;
    builder->container(container_kind, &size);

    // we can only read int values for sizes so we have to manually set the buffer size when reading
    if (builder->mode() == SerializerMode::reading)
    {
        buffer->size = sign_cast<SizeType>(size);
    }

    if (container_kind == SerializedContainerKind::sequential)
    {
        for (SizeType i = 0; i < buffer->size; ++i)
        {
            builder->element(&buffer->data[i]);
        }
    }
    else
    {
        builder->bytes(reinterpret_cast<u8*>(buffer->data), buffer->size);
    }
}


} // namespace bee