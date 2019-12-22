/*
 *  Array.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/IO.hpp"

namespace bee {


struct SerializedContainer
{
    virtual void serialize(SerializationBuilder* builder, void* container) = 0;
};


template <typename T, ContainerMode Mode>
struct SerializedArray final : public SerializedContainer
{
    void serialize(SerializationBuilder* builder, void* container) override
    {
        auto array = static_cast<ArrayType<T, Mode>*>(container);

        int size = array->size();
        builder->container(SerializedContainerKind::sequential, 1, &size);

        if (builder->mode() == SerializerMode::reading)
        {
            array->resize(size);
        }

        for (auto& element : *array)
        {
            builder->element(&element);
        }
    }
};

//template <
//    typename        KeyType,
//    typename        ValueType,
//    ContainerMode   Mode,
//    typename        Hasher,
//    typename        KeyEqual
//>
//void serialize_hashmap(SerializationBuilder* builder, HashMap<KeyType, ValueType, Mode, Hasher, KeyEqual>* map)
//{
//    int size = map->size();
//    builder->version(1).size(&size);
//
//    if (builder->mode() == SerializerMode::reading)
//    {
//        map->rehash(size);
//    }
//
//    String key(temp_allocator());
//
//    if (builder->mode() == SerializerMode::reading)
//    {
//        for (int i = 0; i < size; ++i)
//        {
//            key.clear();
//            builder->key(&key);
//            auto key_value = map->insert(key.view(), ValueType{});
//            builder->element(&key_value->value);
//        }
//    }
//    else
//    {
//        for (KeyValuePair<KeyType, ValueType>& keyval : *map)
//        {
//            key.clear();
//            key.append(keyval.key);
//            builder->key(&key).element(&keyval.value);
//        }
//    }
//}
//
//void serialize_array(SerializationBuilder* builder)
//{
//    auto type = builder->type()->as<RecordType>();
//    auto data_field = find_field(type->fields, "data_");
//
//    BEE_ASSERT_F(data_field != nullptr, "cannot serialize array: missing `data_` field");
//
//    auto size = builder->get_field_data<int>("size_");
//    builder.add
//    for (int i = 0; i < *size; ++i)
//    {
//        serialize_type(type->tem)
//    }
//
//    builder->version(1)
//        .add(1, &size)
//        .add_bytes(1, type)
//}


} // namespace bee