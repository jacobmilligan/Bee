/*
 *  JSONSerializerV2.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/SerializationV2/Serialization.hpp"

#define BEE_RAPIDJSON_READER_H
#define BEE_RAPIDJSON_STRINGBUFFER_H
#define BEE_RAPIDJSON_PRETTYWRITER_H
#define BEE_RAPIDJSON_DOCUMENT_H
#include "Bee/Core/JSON/RapidJSON.hpp"


namespace bee {


class BEE_CORE_API JSONSerializerV2 final : public Serializer
{
public:
    explicit JSONSerializerV2(Allocator* allocator = system_allocator());

    explicit JSONSerializerV2(const char* src, const rapidjson::ParseFlag parse_flags, Allocator* allocator = system_allocator());

    explicit JSONSerializerV2(char* mutable_src, const rapidjson::ParseFlag parse_flags, Allocator* allocator = system_allocator());

    inline const char* c_str() const
    {
        return string_buffer_.GetString();
    }

    BEE_SERIALIZER_INTERFACE(text)
private:
    rapidjson::StringBuffer                             string_buffer_;
    rapidjson::PrettyWriter<rapidjson::StringBuffer>    writer_;
    rapidjson::ParseFlag                                parse_flags_;
    rapidjson::Document                                 reader_doc_;
    DynamicArray<rapidjson::Value*>                     stack_;
    const char*                                         src_ { nullptr };
};


} // namespace bee