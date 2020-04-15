/*
 *  Reader.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Serialization/Serialization.hpp"
#include "Bee/Core/JSON/JSON.hpp"


namespace bee {


class BEE_CORE_API BeeJsonReader final : public Serializer
{
public:
    explicit BeeJsonReader(const json::ParseOptions& parse_options, Allocator* allocator = system_allocator());

    BeeJsonReader(char* src, const json::ParseOptions& parse_options, Allocator* allocator = system_allocator());

    void reset(char* src);

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
    struct DocIterator
    {
        json::ValueType type { json::ValueType::unknown };

        union
        {
            i32                   array_index;
            json::object_iterator object;
        };
    };

    char*                           src_ { nullptr };
    json::Document                  doc_;
    DynamicArray<DocIterator>       iter_stack_;
    DynamicArray<json::ValueHandle> stack_;

    inline void next_element_if_array()
    {
        if (!stack_.empty() && doc_.get_type(stack_.back()) == json::ValueType::array)
        {
            BEE_ASSERT(!iter_stack_.empty() && iter_stack_.back().type == json::ValueType::array);
            ++iter_stack_.back().array_index;
        }
    }

    inline void end_read_scope()
    {
        BEE_ASSERT(!stack_.empty());
        stack_.pop_back();
        next_element_if_array();
    }
};


} // namespace bee