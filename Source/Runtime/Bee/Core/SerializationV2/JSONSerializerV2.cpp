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
    current_member_iter_ = rapidjson::Value::MemberIterator{};
    current_element_ = 0;

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
        auto element = &stack_.back()->GetArray()[current_element_];
        stack_.push_back(element);
    }

    BEE_ASSERT_F(stack_.back()->IsObject(), "Invalid type: %d", stack_.back()->GetType());
}

void JSONSerializerV2::end_record()
{
    if (mode == SerializerMode::writing)
    {
        writer_.EndObject();
    }
    else
    {
        BEE_ASSERT_F(stack_.back()->IsObject(), "Invalid type: %d", stack_.back()->GetType());
        end_read_scope();
    }
}

void JSONSerializerV2::begin_object(i32* member_count)
{
    begin_record(nullptr);
    if (mode == SerializerMode::reading)
    {
        *member_count = static_cast<i32>(stack_.back()->MemberCount());
    }

    current_member_iter_ = stack_.back()->MemberBegin();
}

void JSONSerializerV2::end_object()
{
    end_record();
}

void JSONSerializerV2::begin_array(i32* count)
{
    if (mode == SerializerMode::writing)
    {
        writer_.StartArray();
        return;
    }

    BEE_ASSERT_F(stack_.back()->IsArray(), "Invalid type: %d", stack_.back()->GetType());
    *count = static_cast<i32>(stack_.back()->GetArray().Size());
    current_element_ = 0;
}

void JSONSerializerV2::end_array()
{
    if (mode == SerializerMode::writing)
    {
        writer_.EndArray();
        return;
    }

    BEE_ASSERT_F(stack_.back()->IsArray(), "Invalid type: %d", stack_.back()->GetType());
    end_read_scope();
}

void JSONSerializerV2::serialize_field(const char* name)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Key(name);
        return;
    }
    
    // If current element is not an object then we can't serialize a field
    if (BEE_FAIL_F(stack_.back()->IsObject(), "JSONSerializer: expected object type but got: %d", stack_.back()->GetType()))
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
    if (BEE_FAIL_F(stack_.back()->IsObject(), "JSONSerializer: expected object type but got: %d", stack_.back()->GetType()))
    {
        return;
    }

    str::replace_range(key, 0, key->size(), current_member_iter_->name.GetString(), current_member_iter_->name.GetStringLength());
    ++current_member_iter_;
}

void JSONSerializerV2::serialize_string(io::StringStream* stream)
{
    if (mode == SerializerMode::writing)
    {
        writer_.String(stream->c_str(), stream->size(), true);
        return;
    }

    BEE_ASSERT_F(stack_.back()->IsString(), "JSONSerializer: expected string type but got: %d", stack_.back()->GetType());
    stream->write(StringView { stack_.back()->GetString(), static_cast<i32>(stack_.back()->GetStringLength()) });
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

    if (BEE_CHECK_F(stack_.back()->IsBool(), "JSONSerializer: current field is not a boolean type"))
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

    if (BEE_CHECK_F(stack_.back()->IsInt(), "JSONSerializer: current field is not an integer type"))
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

    if (BEE_CHECK_F(stack_.back()->IsInt(), "JSONSerializer: current field is not an integer type"))
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

    if (BEE_CHECK_F(stack_.back()->IsInt(), "JSONSerializer: current field is not an integer type"))
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

    if (BEE_CHECK_F(stack_.back()->IsInt64(), "JSONSerializer: current field is not a 64-bit integer type"))
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

    if (BEE_CHECK_F(stack_.back()->IsUint(), "JSONSerializer: current field is not an unsigned integer type"))
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

    if (BEE_CHECK_F(stack_.back()->IsUint(), "JSONSerializer: current field is not an unsigned integer type"))
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

    if (BEE_CHECK_F(stack_.back()->IsUint(), "JSONSerializer: current field is not an unsigned integer type"))
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

    if (BEE_CHECK_F(stack_.back()->IsUint64(), "JSONSerializer: current field is not a 64-bit unsigned integer type"))
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

    if (BEE_CHECK_F(stack_.back()->IsString(), "JSONSerializer: current field is not a char type"))
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

    if (BEE_CHECK_F(stack_.back()->IsDouble(), "JSONSerializer: current field is not a floating point type"))
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

    if (BEE_CHECK_F(stack_.back()->IsDouble(), "JSONSerializer: current field is not a floating point type"))
    {
        *data = stack_.back()->GetDouble();
        end_read_scope();
    }
}


} // namespace bee