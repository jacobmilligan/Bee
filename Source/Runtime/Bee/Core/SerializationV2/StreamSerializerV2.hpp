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
        : stream(new_stream)
    {}

    BEE_SERIALIZER_INTERFACE(binary)
};


} // namespace bee