/*
 *  JSONSerializer.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Serialization/Serialization.hpp"

#define BEE_RAPIDJSON_READER_H
#define BEE_RAPIDJSON_STRINGBUFFER_H
#define BEE_RAPIDJSON_PRETTYWRITER_H
#define BEE_RAPIDJSON_DOCUMENT_H
#include "Bee/Core/JSON/RapidJSON.hpp"


namespace bee {


BEE_FLAGS(JSONSerializeFlags, u32)
{
    none                = 0u,
    parse_in_situ       = 1u << 0u,
};


class BEE_CORE_API JSONSerializer final : public Serializer
{
public:
    explicit JSONSerializer(Allocator* allocator = system_allocator());

    explicit JSONSerializer(const char* src, const JSONSerializeFlags parse_flags, Allocator* allocator = system_allocator());

    explicit JSONSerializer(char* mutable_src, const JSONSerializeFlags parse_flags, Allocator* allocator = system_allocator());

    void reset(const char* src, const JSONSerializeFlags parse_flag);

    void reset(char* mutable_src, const JSONSerializeFlags parse_flag);

    inline const char* c_str() const
    {
        return string_buffer_.GetString();
    }

    inline i32 string_size() const
    {
        return sign_cast<i32>(string_buffer_.GetLength());
    }

    inline rapidjson::Document& doc()
    {
        BEE_ASSERT(mode == SerializerMode::reading);
        return reader_doc_;
    }

    size_t offset() override;
    size_t capacity() override;

    bool begin() override;
    void end() override;
    void begin_record(const RecordType& type) override;
    void end_record() override;
    void begin_object(i32* member_count) override;
    void end_object() override;
    void begin_array(i32* count) override;
    void end_array() override;
    void begin_text(i32* length) override;
    void end_text(char* buffer, const i32 size, const i32 capacity) override;
    void begin_bytes(i32* size) override;
    void end_bytes(u8* buffer, const i32 size) override;

    bool serialize_field(const char* name) override;
    void serialize_key(String* key) override;
    void serialize_fundamental(bool* data) override;
    void serialize_fundamental(char* data) override;
    void serialize_fundamental(float* data) override;
    void serialize_fundamental(double* data) override;
    void serialize_fundamental(u8* data) override;
    void serialize_fundamental(u16* data) override;
    void serialize_fundamental(u32* data) override;
    void serialize_fundamental(u64* data) override;
    void serialize_fundamental(i8* data) override;
    void serialize_fundamental(i16* data) override;
    void serialize_fundamental(i32* data) override;
    void serialize_fundamental(i64* data) override;
    void serialize_fundamental(u128* data) override;

private:
    rapidjson::StringBuffer                             string_buffer_;
    rapidjson::PrettyWriter<rapidjson::StringBuffer>    writer_;
    rapidjson::Document                                 reader_doc_;
    DynamicArray<rapidjson::Value*>                     stack_;
    DynamicArray<rapidjson::Value::MemberIterator>      member_iter_stack_;
    DynamicArray<i32>                                   element_iter_stack_;
    String                                              base64_encode_buffer_;
    const char*                                         src_ { nullptr };
    JSONSerializeFlags                                  parse_flags_;
    BEE_PAD(4);

    void next_element_if_array();

    void end_read_scope();

    rapidjson::Value* current_value();

    void begin_record();

    inline rapidjson::Value::MemberIterator& current_member_iter()
    {
        return member_iter_stack_.back();
    }

    inline i32 current_element()
    {
        return element_iter_stack_.back();
    }
};


} // namespace bee