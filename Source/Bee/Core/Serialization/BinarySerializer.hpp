/*
 *  BinarySerializer.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Serialization/Serialization.hpp"

namespace bee {


struct BEE_CORE_API BinarySerializer final : public Serializer
{
    i32                 read_offset { 0 };
    DynamicArray<u8>*   array;

    explicit BinarySerializer(DynamicArray<u8>* target_array)
        : Serializer(SerializerFormat::binary),
          array(target_array)
    {}

    inline void reset(DynamicArray<u8>* target_array)
    {
        read_offset = 0;
        array = target_array;
    }

    size_t offset() override;
    size_t capacity() override;

    bool begin() override;
    void end() override;
    void begin_record(const RecordType& record) override {}
    void end_record() override {}
    void begin_object(i32* member_count) override;
    void end_object() override;
    void begin_array(i32* count) override;
    void end_array() override;
    void begin_text(i32* length) override;
    void end_text(char* buffer, const i32 size, const i32 capacity) override;
    void begin_bytes(i32* size) override;
    void end_bytes(u8* buffer, const i32 size) override;

    void serialize_field(const char* name) override {}
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
};


} // namespace bee