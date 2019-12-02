/*
 *  JSONSerializerV2.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "Bee/Core/Math/Math.hpp"

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
      stack_(allocator),
      src_(src)
{
    // Remove insitu flag if the src string is read-only
    if ((parse_flags & rapidjson::ParseFlag::kParseInsituFlag) != 0)
    {
        parse_flags_ = static_cast<rapidjson::ParseFlag>(parse_flags_ & ~rapidjson::ParseFlag::kParseInsituFlag);
    }
}

JSONSerializerV2::JSONSerializerV2(char* mutable_src, const rapidjson::ParseFlag parse_flags, Allocator* allocator)
    : Serializer(SerializerFormat::text),
      parse_flags_(parse_flags),
      stack_(allocator),
      src_(mutable_src)
{}

bool JSONSerializerV2::begin()
{
    if (mode == SerializerMode::reading)
    {
        stack_.clear();
        reader_doc_.Clear();

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

        stack_.push_back(&reader_doc_);
    }
    else
    {
        string_buffer_.Clear();
        writer_.Reset(string_buffer_);
        writer_.StartObject();
    }

    return true;
}

void JSONSerializerV2::end()
{
    if (mode == SerializerMode::writing)
    {
        writer_.EndObject();
    }
}

void JSONSerializerV2::begin_record(const RecordType* type)
{
    if (mode == SerializerMode::writing)
    {
        writer_.StartObject();
    }
}

void JSONSerializerV2::end_record()
{
    writer_.EndObject();
}

void JSONSerializerV2::serialize_field(const Field& field)
{
    if (mode == SerializerMode::writing)
    {
        writer_.Key(field.name);
        return;
    }

    // If current element is not an object then we can't serialize a field
    if (BEE_FAIL_F(stack_.back()->IsObject(), "JSONSerializer: parent element is not an object type"))
    {
        return;
    }

    const auto member = stack_.back()->FindMember(field.name);
    if (BEE_FAIL_F(member != reader_doc_.MemberEnd(), "JSONSerializer: missing field \"%s\"", field.name))
    {
        return;
    }

    stack_.push_back(&member->value);
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
    }
}


} // namespace bee