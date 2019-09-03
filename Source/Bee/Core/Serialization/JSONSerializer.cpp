/*
 *  JSONSerializer.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Meta.hpp"

#define BEE_RAPIDJSON_RAPIDJSON_H
#define BEE_RAPIDJSON_DOCUMENT_H
#define BEE_RAPIDJSON_ERROR_H
#include "Bee/Core/Serialization/JSONSerializer.hpp"


namespace bee {


JSONWriter::JSONWriter(JSONWriter&& other) noexcept
{
    move_construct(std::forward<JSONWriter>(other));
}

JSONWriter& JSONWriter::operator=(JSONWriter&& other) noexcept
{
    move_construct(std::forward<JSONWriter>(other));
    return *this;
}

void JSONWriter::move_construct(JSONWriter&& other) noexcept
{
    string_buffer_ = std::move(other.string_buffer_);
    writer_.Reset(string_buffer_);
}

bool JSONWriter::begin()
{
    if (BEE_FAIL_F(mode() == SerializerMode::writing, "JSONWriter cannot serialize in `reading` mode"))
    {
        return false;
    }

    string_buffer_.Clear();
    writer_.Reset(string_buffer_);
    writer_.StartObject();
    stack_.push_back(rapidjson::Type::kObjectType);
    return true;
}

void JSONWriter::end()
{
    BEE_ASSERT_F(stack_.size() == 1, "Parsing incomplete: source data was possibly corrupt");
    stack_.pop_back();
    writer_.EndObject();
}

void JSONWriter::array_begin(const char* name)
{
    if (stack_.back() == rapidjson::Type::kObjectType)
    {
        writer_.Key(name);
    }

    writer_.StartArray();
    stack_.push_back(rapidjson::Type::kArrayType);
}

void JSONWriter::array_end()
{
    stack_.pop_back();
    writer_.EndArray();
}

void JSONWriter::convert_begin_type(const char* type_name)
{
    if (stack_.back() != rapidjson::Type::kArrayType)
    {
        writer_.Key(type_name);
    }
    writer_.StartObject();
    stack_.push_back(rapidjson::Type::kObjectType);
}

void JSONWriter::convert_end_type()
{
    if (BEE_FAIL_F(!stack_.empty() && stack_.back() == rapidjson::Type::kObjectType, "Mismatched JSON scopes"))
    {
        return;
    }

    writer_.EndObject();
    stack_.pop_back();
}

void JSONWriter::convert(String* value, const char* name)
{
    convert_cstr(value->data(), value->size(), name);
}

void JSONWriter::convert(Path* path, const char* name)
{
    auto generic_string = path->to_generic_string();
    convert_cstr(generic_string.data(), generic_string.size(), name);
}

void JSONWriter::convert_cstr(char* string, const i32 size, const char* name)
{
    if (stack_.back() != rapidjson::Type::kArrayType)
    {
        writer_.Key(name);
    }

    if (string == nullptr && size == 0)
    {
        writer_.String("");
        return;
    }

    writer_.String(string, size);
}

void JSONWriter::convert(bool* b)
{
    writer_.Bool(*b);
}

void JSONWriter::convert(int* i)
{
    writer_.Int(*i);
}

void JSONWriter::convert(unsigned int* i)
{
    writer_.Uint(*i);
}

void JSONWriter::convert(int64_t* i)
{
    writer_.Int64(*i);
}

void JSONWriter::convert(uint64_t* i)
{
    writer_.Uint64(*i);
}

void JSONWriter::convert(double* d)
{
    writer_.Double(*d);
}

void JSONWriter::convert(const char** str)
{
    writer_.String(*str);
}

/*
 * JSONReader
 */
void JSONReader::reset_source(bee::String* source)
{
    source_ = source;
}

bool JSONReader::begin()
{
    if (BEE_FAIL_F(mode() == SerializerMode::reading, "JSONReader cannot serialize in `writing` mode"))
    {
        return false;
    }

    document_.ParseInsitu(source_->data());
    if (BEE_FAIL_F(!document_.HasParseError(), "JSONReader: unable to parse JSON source: %s", rapidjson::GetParseError_En(document_.GetParseError())))
    {
        return false;
    }

    if (BEE_FAIL_F(document_.IsObject(), "JSONReader: expected object as root element"))
    {
        return false;
    }

    stack_.push_back({ &document_, &document_ });
    return true;
}

void JSONReader::end()
{
    BEE_ASSERT(stack_.empty() || (stack_.size() == 1 && stack_.back().value->IsObject()));
    stack_.pop_back();
}

void JSONReader::convert_begin_type(const char* type_name)
{
    // If the current top of the scope stack isn't an object it should be an array element and we already have it
    if (stack_.back().parent->IsArray() && stack_.back().value->IsObject())
    {
        return;
    }

    auto member = stack_.back().value->FindMember(type_name);
    if (BEE_FAIL_F(member != document_.MemberEnd(), "JSONReader: couldn't find object member with name: %s", type_name))
    {
        return;
    }

    if (BEE_FAIL_F(str::compare(member->name.GetString(), type_name) == 0, "JSONReader: couldn't find object member with name: %s", type_name))
    {
        return;
    }
    
    if (BEE_FAIL_F(member->value.IsObject(), "JSONReader: invalid JSON - expected object"))
    {
        return;
    }

    // Create a new object scope for the serialized type
    stack_.push_back({ stack_.back().value, &member->value });
}

void JSONReader::convert_end_type()
{
    // expect end object or array
    BEE_ASSERT(stack_.back().value->IsObject());
    // We need to pop an object scope if the type serialized was parented to an object and not an array
    if (stack_.back().parent->IsObject())
    {
        stack_.pop_back();
    }
}

void JSONReader::convert(bee::String* string, const char* name)
{
    auto json_value = find_json_value(name);
    if (json_value == nullptr)
    {
        return;
    }

    if (BEE_FAIL_F(json_value->IsString(), "%s is not a valid string key in the JSON source", name))
    {
        return;
    }

    StringView view(json_value->GetString(), json_value->GetStringLength());
    string->clear();
    string->append(view);
}

void JSONReader::convert(Path* path, const char* name)
{
    auto json_value = find_json_value(name);
    if (json_value == nullptr)
    {
        return;
    }

    if (BEE_FAIL_F(json_value->IsString(), "%s is not a valid string key in the JSON source", name))
    {
        return;
    }

    StringView appended(json_value->GetString(), json_value->GetStringLength());
    path->clear();
    path->append(appended);
}

void JSONReader::convert_cstr(char* string, const i32 size, const char* name)
{
    auto json_value = find_json_value(name);
    if (json_value == nullptr)
    {
        return;
    }

    if (BEE_FAIL_F(json_value->IsString(), "%s is not a valid string key in the JSON source", name))
    {
        return;
    }

    str::copy(string, size, json_value->GetString(), json_value->GetStringLength());
}

rapidjson::Value* JSONReader::find_json_value(const char* name)
{
    if (stack_.back().value->IsObject())
    {
        const auto member = stack_.back().value->FindMember(name);
        if (BEE_FAIL_F(member != document_.MemberEnd(), "JSONReader: couldn't find member '%s' in object", name))
        {
            return nullptr;
        }
        return &member->value;
    }

    // Otherwise it's an array element
    return stack_.back().value;
}


} // namespace bee
