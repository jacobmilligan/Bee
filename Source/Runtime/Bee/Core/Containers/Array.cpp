/*
 *  Array.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/SerializationV2/Serialization.hpp"

namespace bee {


struct SerializedTemplate
{

};

void serialize_array(SerializationBuilder* builder)
{
    auto type = builder->type()->as<RecordType>();
    auto data_field = find_field(type->fields, "data_");

    BEE_ASSERT_F(data_field != nullptr, "cannot serialize array: missing `data_` field");

    auto size = builder->get_field_data<int>("size_");
    builder.add
    for (int i = 0; i < *size; ++i)
    {
        serialize_type(type->tem)
    }

    builder->version(1)
        .add(1, &size)
        .add_bytes(1, type)
}


} // namespace bee