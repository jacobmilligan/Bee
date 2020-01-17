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
    sequential,
    key_value,
    text
};

class SerializationBuilder;


struct BEE_CORE_API Serializer
{
    SerializerMode          mode { SerializerMode::reading };
    const SerializerFormat  format {SerializerFormat::unknown };

    explicit Serializer(const SerializerFormat serialized_format);

    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual void begin_record(const RecordType* type) = 0;
    virtual void end_record() = 0;
    virtual void begin_object(i32* member_count) = 0;
    virtual void end_object() = 0;
    virtual void begin_array(i32* count) = 0;
    virtual void end_array() = 0;
    virtual void begin_text(i32* length) = 0;
    virtual void end_text(char* buffer, const i32 size, const i32 capacity) = 0;
    virtual void serialize_field(const char* name) = 0;
    virtual void serialize_key(String* key) = 0;
    virtual void serialize_bytes(void* data, const i32 size) = 0;
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


BEE_CORE_API void serialize_type(Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data);

BEE_CORE_API void serialize_type(Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data, const Span<const Type*>& template_type_arguments);

BEE_CORE_API void serialize_type_append(Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data);

BEE_CORE_API void serialize_type_append(Serializer* serializer, const Type* type, Field::serialization_function_t serialization_function, u8* data, const Span<const Type*>& template_type_arguments);


template <typename T>
void serialize_type(SerializationBuilder* builder, T* data) {}


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
    explicit SerializationBuilder(Serializer* new_serializer, const RecordType* type);

    ~SerializationBuilder();

    template <typename FieldType>
    SerializationBuilder& add_field(const i32 version_added, FieldType* field, const char* field_name)
    {
        return add_field(version_added, limits::max<i32>(), field, field_name);
    }

    template <typename FieldType>
    SerializationBuilder& add_field(const i32 version_added, const i32 version_removed, FieldType* field, const char* field_name)
    {
        BEE_ASSERT_F(container_kind_ == SerializedContainerKind::none, "serialization builder is not configured to build a structure - cannot add fields to non-structure types");

        const auto field_type = get_type<FieldType>();

        if (version_ < version_added || version_ >= version_removed)
        {
            return *this;
        }

        serializer_->serialize_field(field_name);

        if ((field_type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
        {
            SerializationBuilder builder(serializer_, field_type->as<RecordType>());
            serialize_type(&builder, field);
        }
        else
        {
            serialize_type(serializer_, field_type, nullptr, reinterpret_cast<u8*>(field));
        }
        return *this;
    }

    template <typename FieldType>
    SerializationBuilder& remove_field(const i32 version_added, const i32 version_removed, const FieldType& default_value, const char* field_name)
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

        if ((field_type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
        {
            SerializationBuilder builder(serializer_, field_type->as<RecordType>());
            serialize_type(&builder, &removed_data);
        }
        else
        {
            serialize_type(serializer_, field_type, nullptr, reinterpret_cast<u8*>(&removed_data));
        }

        return *this;
    }

    SerializationBuilder& structure(const i32 serialized_version);

    SerializationBuilder& container(const SerializedContainerKind kind, i32* size);

    SerializationBuilder& text(char* buffer, const i32 size, const i32 capacity);

    SerializationBuilder& key(String* data);

    template <typename T>
    SerializationBuilder& element(T* data)
    {
        BEE_ASSERT_F(container_kind_ != SerializedContainerKind::none, "serialization builder is not configured to build a container type");

        auto type = get_type<T>();

        if ((type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
        {
            SerializationBuilder builder(serializer_, type->as<RecordType>());
            serialize_type(&builder, data);
        }
        else
        {
            serialize_type(serializer_, type, nullptr, reinterpret_cast<u8*>(data));
        }

        return *this;
    }

    inline SerializerMode mode() const
    {
        return serializer_->mode;
    }

    inline Serializer* serializer()
    {
        return serializer_;
    }

private:
    Serializer*             serializer_ { nullptr };
    const RecordType*       type_ { nullptr };
    SerializedContainerKind container_kind_ { SerializedContainerKind::none };
    i32                     version_ { -1 };
};


template <typename DataType, typename ReflectedType = DataType>
inline void serialize(const SerializerMode mode, Serializer* serializer, DataType* data)
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

    if ((type->serialization_flags & SerializationFlags::uses_builder) != SerializationFlags::none)
    {
        SerializationBuilder builder(serializer, type->as<RecordType>());
        serialize_type(&builder, data);
    }
    else
    {
        serialize_type(serializer, type, nullptr, reinterpret_cast<u8*>(data));
    }

    serializer->end();
}

/*
 ******************************
 *
 * TypeInstance serialization
 *
 ******************************
 */
inline void serialize_type(SerializationBuilder* builder, TypeInstance* instance)
{
    builder->serializer()->begin_record(get_type_as<TypeInstance, RecordType>());

    builder->serializer()->serialize_field("bee::type");
    u32 type_hash = instance->is_valid() ? instance->type()->hash : get_type<UnknownType>()->hash;
    builder->serializer()->serialize_fundamental(&type_hash);

    if (builder->mode() == SerializerMode::reading)
    {
        const Type* type = get_type(type_hash);
        BEE_ASSERT(!type->is(TypeKind::unknown));

        auto allocator = instance->allocator() != nullptr ? instance->allocator() : system_allocator();
        *instance = std::move(type->create_instance(allocator));
    }

    if (builder->mode() == SerializerMode::reading || instance->is_valid())
    {
        BEE_ASSERT(instance->data() != nullptr);

        auto data = static_cast<u8*>(const_cast<void*>(instance->data()));
        serialize_type_append(builder->serializer(), instance->type(), nullptr, data);
    }
}


/*
 **********************
 *
 * Array serialization
 *
 **********************
 */
template <typename T, ContainerMode Mode>
inline void serialize_type(SerializationBuilder* builder, Array<T, Mode>* array)
{
    int size = array->size();
    builder->container(SerializedContainerKind::sequential, &size);

    if (builder->mode() == SerializerMode::reading)
    {
        array->resize(size);
    }

    for (auto& element : *array)
    {
        builder->element(&element);
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
inline void serialize_type(SerializationBuilder* builder, HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>* map)
{
    int size = map->size();
    builder->container(SerializedContainerKind::key_value, &size);

    if (builder->mode() == SerializerMode::reading)
    {
        KeyValuePair<KeyType, ValueType> key_val{};

        for (int i = 0; i < size; ++i)
        {
            builder->key(&key_val.key);
            builder->element(&key_val.value);
            map->insert(key_val);
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
inline void serialize_type(SerializationBuilder* builder, String* string)
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
inline void serialize_type(SerializationBuilder* builder, StaticString<Size>* string)
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
inline void serialize_type(SerializationBuilder* builder, Path* path)
{
    int size = path->size();
    builder->container(SerializedContainerKind::text, &size);

    if (builder->mode() == SerializerMode::reading)
    {
        path->data_.resize(size);
    }

    builder->text(path->data_.data(), path->data_.size(), path->data_.capacity());
}



} // namespace bee