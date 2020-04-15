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


class BEE_CORE_API JSONSerializer final : public Serializer
{
public:
    explicit JSONSerializer(Allocator* allocator = system_allocator());

    explicit JSONSerializer(const char* src, const rapidjson::ParseFlag parse_flags, Allocator* allocator = system_allocator());

    explicit JSONSerializer(char* mutable_src, const rapidjson::ParseFlag parse_flags, Allocator* allocator = system_allocator());

    void reset(const char* src, const rapidjson::ParseFlag parse_flags);

    void reset(char* mutable_src, const rapidjson::ParseFlag parse_flags);

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

    bool begin() override;
    void end() override;
    void begin_record(const RecordType* type) override;
    void end_record() override;
    void begin_object(i32* member_count) override;
    void end_object() override;
    void begin_array(i32* count) override;
    void end_array() override;
    void begin_text(i32* length) override;
    void end_text(char* buffer, const i32 size, const i32 capacity) override;
    void serialize_field(const char* name) override;
    void serialize_key(String* key) override;
    void serialize_bytes(void* data, const i32 size) override;
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
    rapidjson::ParseFlag                                parse_flags_;
    rapidjson::Document                                 reader_doc_;
    DynamicArray<rapidjson::Value*>                     stack_;
    DynamicArray<rapidjson::Value::MemberIterator>      member_iter_stack_;
    DynamicArray<i32>                                   element_iter_stack_;
    const char*                                         src_ { nullptr };

    inline void next_element_if_array()
    {
        if (!stack_.empty() && stack_.back()->IsArray())
        {
            BEE_ASSERT(!element_iter_stack_.empty());
            ++element_iter_stack_.back();
        }
    }

    inline void end_read_scope()
    {
        BEE_ASSERT(!stack_.empty());
        stack_.pop_back();
        next_element_if_array();
    }

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