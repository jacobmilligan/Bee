/*
 *  JSONSerializer.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Base64.hpp"

#define BEE_RAPIDJSON_ERROR_H
#include "Bee/Core/Serialization/JSONSerializer.hpp"

namespace bee {


BEE_TRANSLATION_TABLE_FUNC(rapidjson_type_to_string, rapidjson::Type, const char*, rapidjson::Type::kNumberType + 1,
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


JSONSerializer::JSONSerializer(Allocator* allocator)
    : Serializer(SerializerFormat::text),
      parse_flags_(JSONSerializeFlags::none),
      stack_(allocator),
      base64_encode_buffer_(allocator)
{}

JSONSerializer::JSONSerializer(const char* src, const JSONSerializeFlags parse_flags, Allocator* allocator)
    : Serializer(SerializerFormat::text),
      parse_flags_(parse_flags),
      stack_(allocator),
      base64_encode_buffer_(allocator)
{
    // Remove insitu flag if the src string is read-only
    reset(src, parse_flags);
}

JSONSerializer::JSONSerializer(char* mutable_src, const JSONSerializeFlags parse_flags, Allocator* allocator)
    : Serializer(SerializerFormat::text),
      parse_flags_(parse_flags),
      stack_(allocator),
      base64_encode_buffer_(allocator)
{
    reset(mutable_src, parse_flags);
}

void JSONSerializer::reset(const char* src, const JSONSerializeFlags parse_flags)
{
    src_ = src;
    parse_flags_ = static_cast<JSONSerializeFlags>(parse_flags & ~JSONSerializeFlags::parse_in_situ);
}

void JSONSerializer::reset(char* mutable_src, const JSONSerializeFlags parse_flags)
{
    src_ = mutable_src;
    parse_flags_ = parse_flags;
}

void JSONSerializer::next_element_if_array()
{
    if (!stack_.empty() && stack_.back()->IsArray())
    {
        BEE_ASSERT(!element_iter_stack_.empty());
        ++element_iter_stack_.back();
    }
}

void JSONSerializer::end_read_scope()
{
    BEE_ASSERT(!stack_.empty());
    if (!stack_.back()->IsArray())
    {
        stack_.pop_back();
    }
    if (!stack_.empty() && stack_.back()->IsArray())
    {
        BEE_ASSERT(!element_iter_stack_.empty());
        ++element_iter_stack_.back();
    }
}

rapidjson::Value * JSONSerializer::current_value()
{
    if (stack_.empty())
    {
        return nullptr;
    }

    if (stack_.back()->IsArray())
    {
        BEE_ASSERT(element_iter_stack_.back() < sign_cast<i32>(stack_.back()->GetArray().Size()));
        return &stack_.back()->GetArray()[element_iter_stack_.back()];
    }

    return stack_.back();
}

/*
 *****************************************
 *
 * Serializer interface implementation
 *
 *****************************************
 */
size_t JSONSerializer::offset()
{
    return 0; // The read and write buffers are both dynamic
}

size_t JSONSerializer::capacity()
{
    return limits::max<size_t>(); // The read and write buffers are both dynamic
}

bool JSONSerializer::begin()
{
    if (mode == SerializerMode::reading)
    {
        stack_.clear();

        if ((parse_flags_ & JSONSerializeFlags::parse_in_situ) != JSONSerializeFlags::none)
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

        if (!reader_doc_.IsObject() && !reader_doc_.IsArray())
        {
            log_error("JSONSerializer: expected object or array as root element");
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

void JSONSerializer::end()
{
    // no-op
}

void JSONSerializer::begin_record(const RecordType& /* type */)
{
    begin_record();
}

void JSONSerializer::begin_record()
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
        if (element_iter_stack_.empty())
        {
            log_error("JSON: expected object at but got array");
            return;
        }

        auto* element = &stack_.back()->GetArray()[current_element()];
        stack_.push_back(element);
    }

    json_validate_type(rapidjson::Type::kObjectType, stack_.back());
}

void JSONSerializer::end_record()
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

void JSONSerializer::begin_object(i32* member_count)
{
    begin_record();

    if (mode == SerializerMode::writing)
    {
        return;
    }

    *member_count = static_cast<i32>(stack_.back()->MemberCount());
    member_iter_stack_.push_back(stack_.back()->MemberBegin());
}

void JSONSerializer::end_object()
{
    end_record();

    if (mode == SerializerMode::reading)
    {
        member_iter_stack_.pop_back();
    }
}

void JSONSerializer::begin_array(i32* count)
{
    if (mode == SerializerMode::writing)
    {
        writer_.StartArray();
        return;
    }

    if (stack_.empty())
    {
        stack_.push_back(&reader_doc_);
    }

    json_validate_type(rapidjson::kArrayType, stack_.back());
    *count = static_cast<i32>(stack_.back()->GetArray().Size());
    element_iter_stack_.push_back(0);
}

void JSONSerializer::end_array()
{
    if (mode == SerializerMode::writing)
    {
        writer_.EndArray();
        return;
    }

    json_validate_type(rapidjson::kArrayType, stack_.back());
    element_iter_stack_.pop_back();
    stack_.pop_back();
}

bool JSONSerializer::serialize_field(const char* name)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Key(name);
        return true;
    }
    
    // If current element is not an object then we can't serialize a field
    if (!json_validate_type(rapidjson::kObjectType, stack_.back()))
    {
        return false;
    }

    const auto member = stack_.back()->FindMember(name);
    if (member == stack_.back()->GetObject().MemberEnd())
    {
        return false;
    }

    stack_.push_back(&member->value);
    return true;
}

void JSONSerializer::serialize_key(String* key)
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

    key->assign({ current_member_iter()->name.GetString(), static_cast<i32>(current_member_iter()->name.GetStringLength()) });
    stack_.push_back(&current_member_iter()->value);
    ++current_member_iter();

    if (*key == "ImRect_Expand")
    {
//        BEE_DEBUG_BREAK();
    }
}

void JSONSerializer::begin_text(i32* length)
{
    if (mode == SerializerMode::writing)
    {
        // JSON doesn't need an explicit length value serialized
        return;
    }

    if (stack_.back()->IsArray())
    {
        stack_.push_back(&stack_.back()->GetArray()[current_element()]);
    }

    json_validate_type(rapidjson::kStringType, stack_.back());
    *length = static_cast<i32>(stack_.back()->GetStringLength());
}

void JSONSerializer::end_text(char* buffer, const i32 size, const i32 capacity)
{
    if (mode == SerializerMode::writing)
    {
        writer_.String(buffer, size);
        return;
    }

    json_validate_type(rapidjson::kStringType, stack_.back());
    str::copy(buffer, capacity, stack_.back()->GetString(), static_cast<i32>(stack_.back()->GetStringLength()));
    end_read_scope();
}

//void JSONSerializer::serialize_enum(const EnumType* type, u8* data)
//{
//
//}


void JSONSerializer::begin_bytes(i32* size)
{
    if (mode == SerializerMode::reading)
    {
        if (stack_.back()->IsArray())
        {
            stack_.push_back(&stack_.back()->GetArray()[current_element()]);
        }

        json_validate_type(rapidjson::kStringType, stack_.back());
        *size = base64_decode_size(StringView(stack_.back()->GetString(), stack_.back()->GetStringLength()));
    }
}

void JSONSerializer::end_bytes(u8* buffer, const i32 size)
{
    if (mode == SerializerMode::writing)
    {
        base64_encode(&base64_encode_buffer_, buffer, size);
        writer_.String(base64_encode_buffer_.c_str());
    }
    else
    {
        json_validate_type(rapidjson::kStringType, stack_.back());
        base64_decode(buffer, size, StringView(stack_.back()->GetString(), stack_.back()->GetStringLength()));
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(bool* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Bool(*data);
        return;
    }

    if (json_validate_type<bool>(current_value()))
    {
        *data = current_value()->GetBool();
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(i8* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Int(*data);
        return;
    }

    if (json_validate_type<int>(current_value()))
    {
        *data = sign_cast<i8>(current_value()->GetInt());
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(i16* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Int(*data);
        return;
    }

    if (json_validate_type<int>(current_value()))
    {
        *data = sign_cast<i16>(current_value()->GetInt());
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(i32* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Int(*data);
        return;
    }

    if (json_validate_type<int>(current_value()))
    {
        *data = current_value()->GetInt();
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(i64* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Int64(*data);
        return;
    }

    if (json_validate_type<int64_t>(current_value()))
    {
        *data = current_value()->GetInt64();
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(u8* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Uint(*data);
        return;
    }

    if (json_validate_type<unsigned>(current_value()))
    {
        *data = sign_cast<u8>(current_value()->GetUint());
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(u16* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Uint(*data);
        return;
    }

    if (json_validate_type<unsigned>(current_value()))
    {
        *data = sign_cast<u16>(current_value()->GetUint());
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(u32* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Uint(*data);
        return;
    }

    if (json_validate_type<uint32_t>(current_value()))
    {
        *data = current_value()->GetUint();
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(u64* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Uint64(*data);
        return;
    }

    if (json_validate_type<uint64_t>(current_value()))
    {
        *data = current_value()->GetUint64();
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(char* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.String(data, 1, true);
        return;
    }

    if (json_validate_type(rapidjson::kStringType, current_value()))
    {
        memcpy(data, current_value()->GetString(), math::min(current_value()->GetStringLength(), 1u));
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(float* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Double(*data);
        return;
    }

    if (json_validate_type<float>(current_value()))
    {
        *data = sign_cast<float>(current_value()->GetDouble());
        end_read_scope();
    }
}

void JSONSerializer::serialize_fundamental(double* data)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Double(*data);
        return;
    }

    if (json_validate_type<double>(current_value()))
    {
        *data = current_value()->GetDouble();
    }
}

void JSONSerializer::serialize_fundamental(u128* data)
{
    static thread_local char buffer[33];

    if (mode == SerializerMode::writing)
    {
        str::format_buffer(buffer, static_array_length(buffer), "%" BEE_PRIxu128, BEE_FMT_u128((*data)));
        writer_.String(buffer, static_array_length(buffer) - 1);
        return;
    }

    if (json_validate_type(rapidjson::kStringType, current_value()))
    {
        *data = str::to_u128(StringView(current_value()->GetString(), current_value()->GetStringLength()));
        end_read_scope();
    }
}


} // namespace bee