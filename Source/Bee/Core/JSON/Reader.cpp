/*
 *  Reader.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/JSON/Reader.hpp"

namespace bee {


BEE_TRANSLATION_TABLE_FUNC(value_type_to_string, json::ValueType, const char*, json::ValueType::unknown,
    "object",
    "array",
    "string",
    "number",
    "boolean",
    "null"
)


BEE_FORCE_INLINE bool validate_type(const json::Document& doc, const json::ValueType expected, const json::ValueHandle& value)
{
    const auto actual = doc.get_type(value);
    return BEE_CHECK_F(expected == actual, "BeeJsonReader: expected %s type but got %s", value_type_to_string(expected), value_type_to_string(actual));
}


BeeJsonReader::BeeJsonReader(const json::ParseOptions& parse_options, Allocator* allocator)
    : Serializer(SerializerFormat::text),
      doc_(parse_options),
      stack_(allocator),
      iter_stack_(allocator)
{}

BeeJsonReader::BeeJsonReader(char* src, const json::ParseOptions& parse_options, Allocator* allocator)
    : BeeJsonReader(parse_options, allocator)
{
    reset(src);
}

void BeeJsonReader::reset(char* src)
{
    iter_stack_.clear();
    stack_.clear();
    src_ = src;
}

bool BeeJsonReader::begin()
{
    if (mode == SerializerMode::writing)
    {
        log_error("BeeJsonReader only supports reading serialized data");
        return false;
    }

    if (!doc_.parse(src_))
    {
        log_error("BeeJsonReader: %s", doc_.get_error_string().c_str());
        return false;
    }

    if (doc_.get_type(doc_.root()) != json::ValueType::object)
    {
        log_error("BeeJsonReader: expected object as root element");
        return false;
    }

    return true;
}

void BeeJsonReader::end()
{
    // no-op
}

void BeeJsonReader::begin_record(const RecordTypeInfo* type)
{
    if (stack_.empty())
    {
        stack_.push_back(doc_.root());
        return;
    }

    if (doc_.get_type(stack_.back()) == json::ValueType::array)
    {
        BEE_ASSERT(iter_stack_.back().type == json::ValueType::array);
        stack_.push_back(doc_.get_element(stack_.back(), iter_stack_.back().array_index));
    }

    validate_type(doc_, json::ValueType::object, stack_.back());
}

void BeeJsonReader::end_record()
{
    validate_type(doc_, json::ValueType::object, stack_.back());
    end_read_scope();
}

void BeeJsonReader::begin_object(i32* member_count)
{
    begin_record(nullptr);
    *member_count = doc_.get_data(stack_.back()).as_size();

    DocIterator iter{};
    iter.type = json::ValueType::object;
    iter.object = doc_.get_members_iterator(stack_.back());

    iter_stack_.push_back(iter);
}

void BeeJsonReader::end_object()
{
    end_record();
    BEE_ASSERT(iter_stack_.back().type == json::ValueType::array);
    iter_stack_.pop_back();
}

void BeeJsonReader::begin_array(i32* count)
{
    validate_type(doc_, json::ValueType::array, stack_.back());
    *count = doc_.get_data(stack_.back()).as_size();

    DocIterator iter{};
    iter.type = json::ValueType::array;
    iter.array_index = 0;
    iter_stack_.push_back(iter);
}

void BeeJsonReader::end_array()
{
    validate_type(doc_, json::ValueType::array, stack_.back());
    end_read_scope();
    iter_stack_.pop_back();
}

void BeeJsonReader::begin_text(i32* length)
{
    if (doc_.get_type(stack_.back()) == json::ValueType::array)
    {
        stack_.push_back(doc_.get_element(stack_.back(), iter_stack_.back().array_index));
    }

    validate_type(doc_, json::ValueType::string, stack_.back());
    *length = str::length(doc_.get_data(stack_.back()).as_string());
}

void BeeJsonReader::end_text(char* buffer, const i32 size, const i32 capacity)
{
    validate_type(doc_, json::ValueType::string, stack_.back());
    str::copy(buffer, capacity, doc_.get_data(stack_.back()).as_string());
    end_read_scope();
}

void BeeJsonReader::serialize_field(const char* name)
{
    // If current element is not an object then we can't serialize a field
    if (!validate_type(doc_, json::ValueType::object, stack_.back()))
    {
        return;
    }

    const auto member = doc_.get_member(stack_.back(), name);

    if (BEE_FAIL_F(member.is_valid(), "JSONSerializer: missing field \"%s\"", name))
    {
        return;
    }

    stack_.push_back(member);
}

void BeeJsonReader::serialize_key(String* key)
{
    // If current element is not an object then we can't serialize a key
    if (!validate_type(doc_, json::ValueType::object, stack_.back()))
    {
        return;
    }

    BEE_ASSERT(iter_stack_.back().type == json::ValueType::object);

    key->append(iter_stack_.back().object->key);
    stack_.push_back(iter_stack_.back().object->value);
    ++iter_stack_.back().object;
}

void BeeJsonReader::serialize_bytes(void* data, const i32 size)
{
    BEE_UNREACHABLE("Not implemented");
}

void BeeJsonReader::serialize_fundamental(bool* data)
{
    if (!validate_type(doc_, json::ValueType::boolean, stack_.back()))
    {
        return;
    }

    *data = doc_.get_data(stack_.back()).as_boolean();
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(char* data)
{
    if (!validate_type(doc_, json::ValueType::string, stack_.back()))
    {
        return;
    }

    auto value = doc_.get_data(stack_.back());
    const auto string_length = str::length(value.as_string());

    if (string_length == 0)
    {
        *data = '\0';
    }
    else
    {
        *data = value.as_string()[0];
    }

    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(float* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = static_cast<float>(doc_.get_data(stack_.back()).as_number());
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(double* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = doc_.get_data(stack_.back()).as_number();
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(u8* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = sign_cast<u8>(doc_.get_data(stack_.back()).as_size());
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(u16* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = sign_cast<u16>(doc_.get_data(stack_.back()).as_size());
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(u32* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = sign_cast<u32>(doc_.get_data(stack_.back()).as_size());
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(u64* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = sign_cast<u64>(doc_.get_data(stack_.back()).as_size());
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(i8* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = static_cast<i8>(doc_.get_data(stack_.back()).as_size());
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(i16* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = static_cast<i16>(doc_.get_data(stack_.back()).as_size());
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(i32* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = doc_.get_data(stack_.back()).as_size();
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(i64* data)
{
    if (!validate_type(doc_, json::ValueType::number, stack_.back()))
    {
        return;
    }

    *data = doc_.get_data(stack_.back()).as_size();
    end_read_scope();
}

void BeeJsonReader::serialize_fundamental(u128* data)
{
    if (!validate_type(doc_, json::ValueType::string, stack_.back()))
    {
        return;
    }

    *data = str::to_u128(doc_.get_data(stack_.back()).as_string());
    end_read_scope();
}


} // namespace bee