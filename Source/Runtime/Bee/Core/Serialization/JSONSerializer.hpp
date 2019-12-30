/*
 *  JSONSerializer.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Serialization/Serialization.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

#define BEE_RAPIDJSON_READER_H
#define BEE_RAPIDJSON_STRINGBUFFER_H
#define BEE_RAPIDJSON_PRETTYWRITER_H
#define BEE_RAPIDJSON_DOCUMENT_H
#include "Bee/Core/JSON/RapidJSON.hpp"

namespace bee {


class BEE_CORE_API JSONWriter : public Serializer, public Noncopyable
{
public:
    explicit JSONWriter(Allocator* allocator = system_allocator())
        : stack_(allocator)
    {}

    JSONWriter(JSONWriter&& other) noexcept;

    JSONWriter& operator=(JSONWriter&& other) noexcept;

    bool begin();

    void end();

    void convert_begin_type(const char* type_name);

    void convert_end_type();

    void convert(bool* b);

    void convert(int* i);

    void convert(unsigned int* i);

    void convert(int64_t* i);

    void convert(uint64_t* i);

    void convert(double* d);

    void convert(const char** str);

    template <typename T>
    inline std::enable_if_t<std::is_enum<T>::value> convert(T* value)
    {
        convert(reinterpret_cast<std::underlying_type_t<T>*>(value));
    }

    template <typename T>
    inline void convert(T* value, const char* name)
    {
        assert_trivial<T>();

        BEE_ASSERT(value != nullptr);
        if (stack_.back() == rapidjson::Type::kObjectType)
        {
            writer_.Key(name);
        }
        convert(value);
    }

    template <typename T, ContainerMode Mode>
    inline void convert(Array<T, Mode>* value, const char* name)
    {
        array_begin(name);
        for (auto& element : *value)
        {
            serialize_type(this, &element, name);
        }
        array_end();
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
        for (auto& element: *map)
        {
            serialize_type(this, &element, &element.key);
            serialize_type(this, &element, &element.value);
        }
    }

    // Convert a String container
    void convert(String* string, const char* name);

    // Convert a Path container
    void convert(Path* path, const char* name);

    template <typename T>
    inline void convert_cbuffer(T* array, const i32 size, const char* name)
    {
        array_begin(name);
        if (array != nullptr)
        {
            for (int elem_idx = 0; elem_idx < size; ++elem_idx)
            {
                serialize_type(this, &array[elem_idx], name);
            }
        }
        array_end();
    }

    void convert_cstr(char* string, i32 size, const char* name);

    inline const char* c_str() const
    {
        return string_buffer_.GetString();
    }

    inline rapidjson::PrettyWriter<rapidjson::StringBuffer>& pretty_writer()
    {
        return writer_;
    }

private:
    DynamicArray<rapidjson::Type>                       stack_;
    rapidjson::StringBuffer                             string_buffer_;
    rapidjson::PrettyWriter<rapidjson::StringBuffer>    writer_;

    void move_construct(JSONWriter&& other) noexcept;

    void array_begin(const char* name);
    void array_end();
};

class BEE_CORE_API JSONReader : public Serializer
{
public:
    explicit JSONReader(bee::String* source, Allocator* allocator = system_allocator())
        : source_(source),
          stack_(allocator)
    {}

    void reset_source(bee::String* source);

    bool begin();

    void end();

    void convert_begin_type(const char* type_name);

    void convert_end_type();

    template <typename T>
    void convert(T* value, const char* name)
    {
        assert_trivial<T>();

        if (BEE_FAIL_F(find_value_in_current_root<T>(name), "JSONReader: mismatched types found in JSON source"))
        {
            return;
        }

        *value = get<T>(name);
    }

    template <typename T, ContainerMode Mode>
    inline void convert(Array<T, Mode>* array, const char* name)
    {
        auto json_value = find_json_value(name);
        if (json_value == nullptr)
        {
            return;
        }

        array->resize(sign_cast<i32>(json_value->GetArray().Size()));
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
        auto json_value = find_json_value(name);
        if (json_value == nullptr)
        {
            return;
        }

        const auto member_size = json_value->GetObject().MemberCount();
        for (int m = 0; m < member_size; ++m)
        {
            KeyValuePair<KeyType, ValueType> keyval{};
            serialize_type(this, &keyval.key, name);
            serialize_type(this, &keyval.value, name);
            map->insert(keyval);
        }
    }

    // Convert a String container
    void convert(bee::String* string, const char* name);

    // Convert a Path container
    void convert(Path* path, const char* name);

    template <typename T>
    void convert_cbuffer(T* buffer, const i32 size, const char* name)
    {
        auto json_value = find_json_value(name);
        if (json_value == nullptr)
        {
            return;
        }

        if (BEE_FAIL_F(json_value->IsArray() && sign_cast<i32>(json_value->GetArray().Size()) == size, "Mismatched array sizes"))
        {
            return;
        }

        auto json_array = json_value->GetArray();
        for (int array_idx = 0; array_idx < size; ++array_idx)
        {
            stack_.push_back({ json_value, &json_array[array_idx] });
            serialize_type(this, &buffer[array_idx], name);
            stack_.pop_back();
        }
    }

    void convert_cstr(char* string, i32 size, const char* name);

    inline rapidjson::Document& document()
    {
        return document_;
    }

private:
    struct Scope
    {
        rapidjson::Value* parent { nullptr };
        rapidjson::Value* value { nullptr };
    };

    rapidjson::Document     document_;
    bee::String*            source_ { nullptr };
    DynamicArray<Scope>     stack_;

    template <typename T>
    inline bool find_value_in_current_root(const char* name)
    {
        if (!stack_.back().value->IsObject())
        {
            return stack_.back().value->Is<T>();
        }

        if (BEE_FAIL_F(stack_.back().value->MemberCount() > 0, "Expected JSON object member: %s", name))
        {
            return false;
        }

        const auto member_iter = stack_.back().value->FindMember(name);
        return member_iter != document_.MemberEnd() && member_iter->value.Is<T>();
    }

    template <typename T>
    inline T get(const char* name)
    {
        if (stack_.back().value->IsObject())
        {
            return stack_.back().value->FindMember(name)->value.Get<T>();
        }

        return stack_.back().value->Get<T>();
    }

    rapidjson::Value* find_json_value(const char* name);
};


} // namespace bee
