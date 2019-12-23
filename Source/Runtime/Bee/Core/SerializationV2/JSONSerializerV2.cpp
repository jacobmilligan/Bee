/*
 *  JSONSerializerV2.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/IO.hpp"

#define BEE_RAPIDJSON_ERROR_H
#include "Bee/Core/SerializationV2/JSONSerializerV2.hpp"

namespace bee {


BEE_TRANSLATION_TABLE(rapidjson_type_to_string, rapidjson::Type, const char*, rapidjson::Type::kNumberType + 1,
    "null",
    "false",
    "true",
    "object",
    "array",
    "string",
    "number"
)

template <typename T>
BEE_FORCE_INLINE bool json_validate_type(const rapidjson::Value* value)
{
    return BEE_CHECK_F(value->Is<T>(), "JSONSerializer: expected %s type but got %s", get_type<T>()->name, rapidjson_type_to_string(value->GetType()));
}

BEE_FORCE_INLINE bool json_validate_type(const rapidjson::Type type, const rapidjson::Value* value)
{
    return BEE_CHECK_F(type == value->GetType(), "JSONSerializer: expected %s type but got %s", rapidjson_type_to_string(type), rapidjson_type_to_string(value->GetType()));
}


JSONSerializerV2::JSONSerializerV2(Allocator* allocator)
    : Serializer(SerializerFormat::text),
      parse_flags_(static_cast<rapidjson::ParseFlag>(-1)),
      stack_(allocator)
{}

JSONSerializerV2::JSONSerializerV2(const char* src, const rapidjson::ParseFlag parse_flags, Allocator* allocator)
    : Serializer(SerializerFormat::text),
      parse_flags_(parse_flags),
      stack_(allocator)
{
    // Remove insitu flag if the src string is read-only
    reset(src, parse_flags);
}

JSONSerializerV2::JSONSerializerV2(char* mutable_src, const rapidjson::ParseFlag parse_flags, Allocator* allocator)
    : Serializer(SerializerFormat::text),
      parse_flags_(parse_flags),
      stack_(allocator)
{
    reset(mutable_src, parse_flags);
}

void JSONSerializerV2::reset(const char* src, const rapidjson::ParseFlag parse_flags)
{
    src_ = src;
    parse_flags_ = static_cast<rapidjson::ParseFlag>(parse_flags & ~rapidjson::ParseFlag::kParseInsituFlag);
}

void JSONSerializerV2::reset(char* mutable_src, const rapidjson::ParseFlag parse_flags)
{
    src_ = mutable_src;
    parse_flags_ = parse_flags;
}

bool JSONSerializerV2::begin()
{
    if (mode == SerializerMode::reading)
    {
        stack_.clear();

        if ((parse_flags_ & rapidjson::ParseFlag::kParseInsituFlag) != 0)
        {
            reader_doc_.ParseInsitu(const_cast<char*>(src_));
        }
        else
        {
            reader_doc_.Parse(src_);
        }

        if (reader_doc_.HasParseError())
        {
            log_error("JSONSerializer parse error: %s", rapidjson::GetParseError_En(reader_doc_.GetParseError()));
            return false;
        }

        if (!reader_doc_.IsObject())
        {
            log_error("JSONSerializer: expected object as root element");
            return false;
        }
    }
    else
    {
        string_buffer_.Clear();
        writer_.Reset(string_buffer_);
    }

    return true;
}

void JSONSerializerV2::end()
{
    // no-op
}

void JSONSerializerV2::begin_record(const RecordType* /* type */)
{
    if (mode == SerializerMode::writing)
    {
        writer_.StartObject();
        return;
    }

    if (stack_.empty())
    {
        stack_.push_back(&reader_doc_);
        return;
    }

    if (stack_.back()->IsArray())
    {
        auto element = &stack_.back()->GetArray()[current_element()];
        stack_.push_back(element);
    }

    json_validate_type(rapidjson::Type::kObjectType, stack_.back());
}

void JSONSerializerV2::end_record()
{
    if (mode == SerializerMode::writing)
    {
        writer_.EndObject();
    }
    else
    {
        json_validate_type(rapidjson::kObjectType, stack_.back());
        end_read_scope();
    }
}

void JSONSerializerV2::begin_object(i32* member_count)
{
    begin_record(nullptr);

    if (mode == SerializerMode::writing)
    {
        return;
    }

    *member_count = static_cast<i32>(stack_.back()->MemberCount());
    member_iter_stack_.push_back(stack_.back()->MemberBegin());
}

void JSONSerializerV2::end_object()
{
    end_record();

    if (mode == SerializerMode::reading)
    {
        member_iter_stack_.pop_back();
    }
}

void JSONSerializerV2::begin_array(i32* count)
{
    if (mode == SerializerMode::writing)
    {
        writer_.StartArray();
        return;
    }

    json_validate_type(rapidjson::kArrayType, stack_.back());
    *count = static_cast<i32>(stack_.back()->GetArray().Size());
    element_iter_stack_.push_back(0);
}

void JSONSerializerV2::end_array()
{
    if (mode == SerializerMode::writing)
    {
        writer_.EndArray();
        return;
    }

    json_validate_type(rapidjson::kArrayType, stack_.back());
    end_read_scope();
    element_iter_stack_.pop_back();
}

void JSONSerializerV2::serialize_field(const char* name)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Key(name);
        return;
    }
    
    // If current element is not an object then we can't serialize a field
    if (!json_validate_type(rapidjson::kObjectType, stack_.back()))
    {
        return;
    }

    const auto member = stack_.back()->FindMember(name);

    if (BEE_FAIL_F(member != reader_doc_.MemberEnd(), "JSONSerializer: missing field \"%s\"", name))
    {
        return;
    }

    stack_.push_back(&member->value);
}

void JSONSerializerV2::serialize_key(String* key)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Key(key->c_str(), key->size());
        return;
    }

    // If current element is not an object then we can't serialize a key
    if (!json_validate_type(rapidjson::kObjectType, stack_.back()))
    {
        return;
    }

    key->append({ current_member_iter()->name.GetString(), static_cast<i32>(current_member_iter()->name.GetStringLength()) });
    stack_.push_back(&current_member_iter()->value);
    ++current_member_iter();
}

void JSONSerializerV2::begin_text(i32* length)
{
    if (mode == SerializerMode::writing)
    {
        // JSON doesn't need an explicit length value serialized
        return;
    }

    json_validate_type(rapidjson::kStringType, stack_.back());
    *length = static_cast<i32>(stack_.back()->GetStringLength());
}

void JSONSerializerV2::end_text(char* buffer, const i32 size, const i32 capacity)
{
    if (mode == SerializerMode::writing)
    {
        writer_.String(buffer, size);
        return;
    }

    json_validate_type(rapidjson::kStringType, stack_.back());
    str::copy(buffer, capacity, stack_.back()->GetString(), static_cast<i32>(stack_.back()->GetStringLength()));
}

//void JSONSerializerV2::serialize_enum(const EnumType* type, u8* data)
//{
//
//}


void JSONSerializerV2::serialize_bytes(void* data, const i32 size)
{
    BEE_UNREACHABLE("Not implemented");
}

void JSONSerializerV2::serialize_fundamental(bool* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Bool(*data);
        return;
    }

    if (json_validate_type<bool>(stack_.back()))
    {
        *data = stack_.back()->GetBool();
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(i8* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Int(*data);
        return;
    }

    if (json_validate_type<int>(stack_.back()))
    {
        *data = sign_cast<i8>(stack_.back()->GetInt());
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(i16* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Int(*data);
        return;
    }

    if (json_validate_type<int>(stack_.back()))
    {
        *data = sign_cast<i16>(stack_.back()->GetInt());
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(i32* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Int(*data);
        return;
    }

    if (json_validate_type<int>(stack_.back()))
    {
        *data = stack_.back()->GetInt();
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(i64* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Int64(*data);
        return;
    }

    if (json_validate_type<int64_t>(stack_.back()))
    {
        *data = stack_.back()->GetInt64();
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(u8* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Uint(*data);
        return;
    }

    if (json_validate_type<unsigned>(stack_.back()))
    {
        *data = sign_cast<u8>(stack_.back()->GetUint());
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(u16* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Uint(*data);
        return;
    }

    if (json_validate_type<unsigned>(stack_.back()))
    {
        *data = sign_cast<u16>(stack_.back()->GetUint());
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(u32* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Uint(*data);
        return;
    }

    if (json_validate_type<uint32_t>(stack_.back()))
    {
        *data = stack_.back()->GetUint();
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(u64* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Uint64(*data);
        return;
    }

    if (json_validate_type<uint64_t>(stack_.back()))
    {
        *data = stack_.back()->GetUint64();
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(char* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.String(data, 1, true);
        return;
    }

    if (json_validate_type(rapidjson::kStringType, stack_.back()))
    {
        memcpy(data, stack_.back()->GetString(), math::min(stack_.back()->GetStringLength(), 1u));
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(float* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Double(*data);
        return;
    }

    if (json_validate_type<float>(stack_.back()))
    {
        *data = sign_cast<float>(stack_.back()->GetDouble());
        end_read_scope();
    }
}

void JSONSerializerV2::serialize_fundamental(double* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Double(*data);
        return;
    }

    if (json_validate_type<double>(stack_.back()))
    {
        *data = stack_.back()->GetDouble();
        end_read_scope();
    }
}


} // namespace bee