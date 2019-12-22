/*
 *  StreamSerializer.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/IO.hpp"
#include "Bee/Core/SerializationV2/Serialization.hpp"

namespace bee {


struct BEE_CORE_API StreamSerializerV2 final : public Serializer
{
    io::Stream* stream;

    explicit StreamSerializerV2(io::Stream* new_stream)
        : Serializer(SerializerFormat::binary),
          stream(new_stream)
    {}

    bool begin() override;
    void end() override;
    void begin_record(const RecordType* type) override {}
    void end_record() override {}
    void begin_object(i32* member_count) override;
    void end_object() override {}
    void begin_array(i32* count) override;
    void end_array() override {}
    void serialize_field(const char* name) override;
    void serialize_key(String* key) override;
    void serialize_bytes(void* data, const i32 size) override;
    void serialize_fundamental(bool* data) override;
    void serialize_fundamental(char* data) override;
    void serialize_fundamental(float* data) override;
    void serialize_fundamental(double* data) override;
    void serialize_fundamental(bee::u8* data) override;
    void serialize_fundamental(bee::u16* data) override;
    void serialize_fundamental(bee::u32* data) override;
    void serialize_fundamental(bee::u64* data) override;
    void serialize_fundamental(bee::i8* data) override;
    void serialize_fundamental(bee::i16* data) override;
    void serialize_fundamental(bee::i32* data) override;
    void serialize_fundamental(bee::i64* data) override;
};


} // namespace bee