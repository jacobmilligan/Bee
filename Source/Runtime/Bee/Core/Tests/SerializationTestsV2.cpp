/*
 *  SerializationTestsV2.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Tests/SerializationTestsTypes.hpp"

#include <Bee/Core/SerializationV2/Serialization.hpp>
#include <Bee/Core/SerializationV2/JSONSerializerV2.hpp>
#include <Bee/Core/SerializationV2/StreamSerializerV2.hpp>
#include <Bee/Core/IO.hpp>

#include <gtest/gtest.h>
#include <rapidjson/filereadstream.h>

using namespace bee;



TEST(SerializationTestsV2, primitives)
{
    PrimitivesStruct test_struct{};
    test_struct.intval = 23;
    test_struct.uval = 100;
    test_struct.charval = 'j';
    test_struct.boolval = false;
    test_struct.ubyteval = 100;
    test_struct.ibyteval = 64;

    PrimitivesStructV2 test_struct_v2 {};
    test_struct_v2.intval = 23;
    test_struct_v2.uval = 100;
    test_struct_v2.charval = 'j';
    test_struct_v2.boolval = false;
    test_struct_v2.ibyteval = 64;

    // Test writing to buffer
    DynamicArray<u8> buffer;
    io::MemoryStream stream(&buffer);

    StreamSerializerV2 serializer(&stream);
    serialize(SerializerMode::writing, &serializer, &test_struct);

    // Version 1 of the primitive struct has the `is_valid` field serialized
    const auto sizeof_record_header = sizeof(i32) * 2;
    const auto expected_buffer_size = sizeof_record_header + sizeof(int) + sizeof(u32) + sizeof(char) + sizeof(bool)
                                    + sizeof(u8) + sizeof(i8) + sizeof(u32) * 2 * 6;
    ASSERT_EQ(buffer.size(), expected_buffer_size);

    stream.seek(0, io::SeekOrigin::begin);
    int version = -1;
    int field_count = -1;
    u64 header = 0;
    PrimitivesStruct read_struct{};
    stream.read(&version, sizeof(int));
    stream.read(&field_count, sizeof(int));

    for (const Field& field : get_type_as<PrimitivesStruct, RecordType>()->fields)
    {
        if (field.version_added <= 0)
        {
            continue;
        }
        stream.read(&header, sizeof(u32) * 2);
        stream.read(reinterpret_cast<u8*>(&read_struct) + field.offset, field.type->size);
    }

    ASSERT_EQ(version, serializer.version);
    ASSERT_EQ(field_count, 6);
    ASSERT_EQ(read_struct, test_struct);

    // Test reading from written buffer into a struct
    new (&read_struct) PrimitivesStruct{};
    serialize(SerializerMode::reading, &serializer, &read_struct);
    ASSERT_EQ(read_struct, test_struct);

    /*
     * Write out the test struct again but this time with a different revision - do this by calling
     * serialize_type directly with a modified type struct
     */
    serialize(SerializerMode::writing, &serializer, &test_struct_v2);

    // Version 2 is missing the ubyteval field
    ASSERT_EQ(buffer.size(), expected_buffer_size - sizeof(u32));

    stream.seek(0, io::SeekOrigin::begin);
    version = -1;
    field_count = -1;
    PrimitivesStructV2 read_struct_v2{};
    stream.read(&version, sizeof(int));
    stream.read(&field_count, sizeof(int));

    for (const Field& field : get_type_as<PrimitivesStructV2, RecordType>()->fields)
    {
        if (field.version_added <= 0)
        {
            continue;
        }
        stream.read(&header, sizeof(u32) * 2);
        stream.read(reinterpret_cast<u8*>(&read_struct_v2) + field.offset, field.type->size);
    }

    ASSERT_EQ(field_count, 5);
    ASSERT_EQ(version, serializer.version);

    // Version 2 contains a removed field - test to check that the default value is written out to the buffer
    ASSERT_EQ(read_struct_v2, test_struct_v2);

    // Read the revision back into the read struct and test all values
    new (&read_struct_v2) PrimitivesStructV2{};
    serialize(SerializerMode::reading, &serializer, &read_struct_v2);
    ASSERT_EQ(read_struct_v2, test_struct_v2);
}


TEST(SerializationTestsV2, complex_type)
{
    char json_buffer[] = R"({
    {
        "bee::version": 1,
        "value": 25,
        {
            "bee::version": 1,
            "is_active": true,
            {
                "bee::version": 1,
                "id_values": [
                    {
                        "bee::version": 1,
                        "value": 0
                    },
                    {
                        "bee::version": 1,
                        "value": 1
                    },
                    {
                        "bee::version": 1,
                        "value": 2
                    },
                    {
                        "bee::version": 1,
                        "value": 3
                    },
                    {
                        "bee::version": 1,
                        "value": 4
                    }
                ]
            }
        }
    }
})";
    String json_str(json_buffer);
    bee::JSONSerializerV2 serializer(json_str.data(), rapidjson::ParseFlag::kParseInsituFlag);
    TestStruct test{};
    serialize(SerializerMode::reading, &serializer, &test);
    ASSERT_EQ(test.value, 25);
    ASSERT_TRUE(test.settings.is_active);
    for (u32 i = 0; i < 5; ++i)
    {
        ASSERT_EQ(test.settings.nested.id_values[i].value, i);
    }

    serialize(SerializerMode::writing, &serializer, &test);
    ASSERT_STREQ(serializer.c_str(), json_buffer);
}

template <typename T>
void assert_serialized_datav2(u8* data_begin, const T& expected, i32* serialized_size)
{
    ASSERT_EQ(*reinterpret_cast<T*>(data_begin), expected);
    *serialized_size = sizeof(T);
}

void assert_serialized_datav2(u8* data_begin, const Path& expected, i32* serialized_size)
{
    // size
    ASSERT_EQ(*reinterpret_cast<i32*>(data_begin), expected.size());
    // data
    const auto data_ptr = reinterpret_cast<char*>(data_begin + sizeof(i32));
    const auto serialized_string = StringView(data_ptr, expected.size());
    ASSERT_EQ(serialized_string, expected.view());
    *serialized_size = sizeof(i32) + serialized_string.size();
}

void assert_serialized_datav2(u8* data_begin, const String& expected, i32* serialized_size)
{
    // size
    ASSERT_EQ(*reinterpret_cast<i32*>(data_begin), expected.size());
    // data
    const auto data_ptr = reinterpret_cast<char*>(data_begin + sizeof(i32));
    const auto serialized_string = StringView(data_ptr, expected.size());
    ASSERT_EQ(serialized_string, expected);
    *serialized_size = sizeof(i32) + serialized_string.size();
}

template <typename T, ContainerMode Mode>
void assert_serialized_datav2(u8* data_begin, const Array<T, Mode>& expected, i32* serialized_size)
{
    // size
    ASSERT_EQ(*reinterpret_cast<i32*>(data_begin), expected.size());

    // data
    const auto data_ptr = data_begin + sizeof(i32);
    i32 cursor = 0;
    for (int elem_idx = 0; elem_idx < expected.size(); ++elem_idx)
    {
        i32 element_size = 0;
        assert_serialized_datav2(data_ptr + cursor, expected[elem_idx], &element_size);
        cursor += element_size;
    }

    *serialized_size = sizeof(i32) + cursor;
}

//TEST(SerializationTestsV2, core_types)
//{
//    bee::MemorySerializer::buffer_t buffer;
//    bee::MemorySerializer serializer(&buffer);
//
//    String str = "Jacob";
//    DynamicArray<String> string_array = { "Jacob", "Is", "Cool" };
//    FixedArray<FixedArray<int>> int_2d = { { 1, 2, 3}, { 4, 5, 6 }, { 7, 8, 9 } };
//
//    // Test String for read/write
//    serialize(SerializerMode::writing, &serializer, &str);
//    i32 serialized_size = 0;
//    assert_serialized_datav2(buffer.data(), str, &serialized_size);
//
//    String deserialized_string;
//    serialize(SerializerMode::reading, &serializer, &deserialized_string);
//
//    ASSERT_EQ(deserialized_string, str);
//
//    // Test dynamic array of strings for read/write
//    serialize(SerializerMode::writing, &serializer, &string_array);
//    assert_serialized_datav2(buffer.begin(), string_array, &serialized_size);
//
//    DynamicArray<String> deserialized_string_array;
//    serialize(SerializerMode::reading, &serializer, &deserialized_string_array);
//
//    ASSERT_EQ(deserialized_string_array.size(), string_array.size());
//    for (int str_idx = 0; str_idx < string_array.size(); ++str_idx)
//    {
//        ASSERT_EQ(deserialized_string_array[str_idx], string_array[str_idx]);
//    }
//
//    // Test multi-dimensional fixed array of fixed arrays of ints for read/write
//    serialize(SerializerMode::writing, &serializer, &int_2d);
//    assert_serialized_datav2(buffer.begin(), int_2d, &serialized_size);
//
//    FixedArray<FixedArray<int>> deserialized_int_2d;
//    serialize(SerializerMode::reading, &serializer, &deserialized_int_2d);
//    ASSERT_EQ(deserialized_int_2d.size(), int_2d.size());
//    for (int array_idx = 0; array_idx < int_2d.size(); ++array_idx)
//    {
//        ASSERT_EQ(deserialized_int_2d[array_idx].size(), int_2d[array_idx].size());
//        for (int int_idx = 0; int_idx < int_2d[array_idx].size(); ++int_idx)
//        {
//            ASSERT_EQ(deserialized_int_2d[array_idx][int_idx], int_2d[array_idx][int_idx]);
//        }
//    }
//
//    // Test paths
//    auto test_path = Path::executable_path();
//    serialize(SerializerMode::writing, &serializer, &test_path);
//    assert_serialized_datav2(buffer.begin(), test_path, &serialized_size);
//
//    // Test hashmaps
//    DynamicHashMap<String, int> expected_map;
//    expected_map.insert("one", 1);
//    expected_map.insert("two", 2);
//    expected_map.insert("three", 3);
//    expected_map.insert("four", 4);
//    expected_map.insert("five", 5);
//    serialize(SerializerMode::writing, &serializer, &expected_map);
//
//    DynamicHashMap<String, int> actual_map;
//    serialize(SerializerMode::reading, &serializer, &actual_map);
//
//    ASSERT_EQ(actual_map.size(), expected_map.size());
//    for (auto& val : expected_map)
//    {
//        const auto found = actual_map.find(val.key);
//        ASSERT_NE(found, nullptr);
//        ASSERT_EQ(val.key, found->key);
//        ASSERT_EQ(val.value, found->value);
//    }
//}
