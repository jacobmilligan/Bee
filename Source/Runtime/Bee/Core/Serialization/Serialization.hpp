/*
 *  Serialization.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/String.hpp"


namespace bee {


/*
 ***************************
 *
 * Serialization
 *
 ***************************
 */
#define BEE_DEFINE_SERIALIZE_TYPE(current_version, type, type_name)                                 \
    template <typename SerializerType>                                                              \
    inline void serialize_convert(SerializerType* serializer, type* data, const char* name);        \
                                                                                                    \
    template <typename SerializerType>                                                              \
    inline void serialize_type(SerializerType* serializer, type* data, const char* name)            \
    {                                                                                               \
        if (serializer->mode() == bee::SerializerMode::writing)                                     \
        {                                                                                           \
            serializer->version = current_version;                                                  \
        }                                                                                           \
        serializer->convert_begin_type(type_name);                                                  \
        serialize_type(serializer, &serializer->version, "bee::version");                           \
        serialize_convert(serializer, data, name);                                                  \
        serializer->convert_end_type();                                                             \
    }                                                                                               \
                                                                                                    \
    template <typename SerializerType>                                                              \
    inline void serialize_convert(SerializerType* serializer, type* data, const char* name)

#define BEE_SERIALIZE(current_version, type) BEE_DEFINE_SERIALIZE_TYPE(current_version, type, #type)

/**
 * Adds a class/struct field to be serialized alongside a version number specifying which version of the structure
 * the field was added in. For primitive types or types stored on the stack, call `serialize` directly
 */
#define BEE_ADD_FIELD(version_added, field)                     \
    BEE_BEGIN_MACRO_BLOCK                                       \
        if (serializer->version >= (version_added))             \
        {                                                       \
            serialize_type(serializer, &(data->field), #field); \
        }                                                       \
    BEE_END_MACRO_BLOCK

/**
 * Marks a field with type `field_type` as removed from a serialized structure. The version that the field was added
 * and removed in must be specified alongside a default value to write/read if a version with the removed field is
 * being serialized
 */
#define BEE_REMOVE_FIELD(version_added, version_removed, field_type, field_name, default_value_if_present)      \
    BEE_BEGIN_MACRO_BLOCK                                                                                       \
        field_type bee_removed_field = default_value_if_present;                                                \
        if (serializer->version >= (version_added) && serializer->version < (version_removed))                  \
        {                                                                                                       \
            serialize_type(serializer, &(bee_removed_field), #field_name);                                      \
        }                                                                                                       \
    BEE_END_MACRO_BLOCK

/**
 * Adds a serialized integer that ensures integrity between versions by asserting if the value read/written
 * is different to the serializers current `check_integrity_counter` value. Integrity checks should be used liberally
 * to guard against programmer error and always at least added before data for a new version is added, i.e.
 * `BEE_ADD_INTEGRITY_CHECK(NEW_REVISION)`
 */
#define BEE_ADD_INTEGRITY_CHECK(version_added)                                          \
    BEE_BEGIN_MACRO_BLOCK                                                               \
        if (serializer->version >= (version_added))                                     \
        {                                                                               \
            auto check_counter = serializer->check_integrity_counter;                   \
            serialize_type(serializer, &check_counter, "check_integrity_counter");      \
            BEE_ASSERT_F(                                                               \
                check_counter == serializer->check_integrity_counter,                   \
                "serialization integrity check failed (%d != %d). This can  happen if " \
                "the serializer_ is using a different version than the data being "      \
                "read/written or `serializer_->check_integrity_counter` was corrupted "  \
                "or modified incorrectly",                                              \
                check_counter, serializer->check_integrity_counter                      \
            );                                                                          \
            ++serializer->check_integrity_counter;                                      \
        }                                                                               \
    BEE_END_MACRO_BLOCK

enum class SerializerMode
{
    reading,
    writing
};


/**
 * # Serializer
 *
 * For a class to be compatible with the BEE_SERIALIZE API it must derive from `Serializer` and
 * implement the following interface with all of the functions present:
 *
 * ```
     interface Serializer
     {
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

         // Called when serializing a c-style array or a buffer with a constant size
         template <typename T>
         void convert_cbuffer(T* array, const i32 size, const char* name);

         // Called when converting a c-style string of characters
         void convert_cstr(char* string, i32 size, const char* name);
     }
 * ```
 *
 * Commonly a serializer specializes the convert<T> function for a few of the core Container types such as:
 *
 * ```
    // Convert either a fixed or dynamic array (using the resize() function if in reading mode)
    template <typename T, ContainerMode Mode>
    void convert(Array<T, Mode>* array, const char* name);

    // Convert a hash map
    template <
      typename        KeyType,
      typename        ValueType,
      ContainerMode   Mode,
      typename        Hasher,
      typename        KeyEqual
    >
    void convert(HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>* map, const char* name);

    // Convert a String container
    void convert(String* string, const char* name);

    // Convert a Path container
    void convert(Path* path, const char* name);
 *  ```
 *
 *  The three serializer classes provided under `Bee/Core/Serialization/` all provide these specializations
 *  so it's usually enough for a custom serializer to use one of these as a backing serializer or base class
 */
struct BEE_CORE_API Serializer
{
    i32             version { 0 };
    i32             check_integrity_counter { 0 };

    void reset(const SerializerMode new_mode)
    {
        version = 0;
        check_integrity_counter = 0;
        mode_ = new_mode;
    }

    inline SerializerMode mode() const
    {
        return mode_;
    }

    template <typename T>
    inline void assert_trivial()
    {
        static_assert(std::is_trivially_copyable<T>::value && !std::is_pointer<T>::value, BEE_STATIC_ASSERT_MSG(
                "Type `T` is not trivial (cannot be trivially copied or is a pointer type)",
                "Consider adding a `convert<T>` specialization to the serializer to provide specialized serialization "
                "behaviour for this type."
        ));
    }
private:
    SerializerMode mode_ { SerializerMode::reading };
};


template <typename SerializerType, typename DataType>
inline void serialize_type(SerializerType* serializer, DataType* data, const char* name)
{
    BEE_ASSERT(serializer != nullptr);
    serializer->convert(data, name);
}

template <typename SerializerType, typename DataType>
inline void serialize_type(SerializerType* serializer, const DataType* data, const char* name)
{
    BEE_ASSERT(serializer != nullptr);
    serialize_type(serializer, const_cast<DataType*>(data), name);
}


template <typename SerializerType, typename DataType>
inline void serialize(const SerializerMode mode, SerializerType* serializer, DataType* data)
{
    serializer->reset(mode);
    if (BEE_FAIL_F(serializer->begin(), "Failed to initialize serialization"))
    {
        return;
    }
    serialize_type(serializer, data, "");
    serializer->end();
}

/*
 ****************************************************
 *
 * Specializations for serializing more complex data
 *
 ****************************************************
 */

/**
 * Serializes statically allocated c-style array
 */
template <typename SerializerType, typename ElementType, i32 Size>
inline void serialize_type(SerializerType* serializer, ElementType(*data)[Size], const char* name)
{
    BEE_ASSERT(serializer != nullptr);
    serializer->convert_cbuffer(&((*data)[0]), Size, name);
}

template <typename SerializerType, i32 Size>
inline void serialize_type(SerializerType* serializer, char(*data)[Size], const char* name)
{
    BEE_ASSERT(serializer != nullptr);
    serializer->convert_cstr(&((*data)[0]), *data == nullptr ? 0 : str::length(*data), name);
}

template <typename SerializerType>
inline void serialize_type(SerializerType* serializer, const char** data, const char* name)
{
    BEE_ASSERT(serializer != nullptr);
    serializer->convert_cstr(const_cast<char*>(*data), *data == nullptr ? 0 : str::length(*data), name);
}

BEE_SERIALIZE(1, Type)
{
    BEE_ADD_FIELD(1, hash);
    BEE_ADD_FIELD(1, size);
    BEE_ADD_FIELD(1, alignment);

    if (serializer->mode() == SerializerMode::reading)
    {
        // Fixup string pointers
        const auto registered_type = get_type(data->hash);
        if (registered_type.is_valid())
        {
            *data = registered_type;
        }
    }
    else
    {
        BEE_ADD_FIELD(1, annotated_name);
        BEE_ADD_FIELD(1, fully_qualified_name);
        BEE_ADD_FIELD(1, name);
    }
}


} // namespace bee


