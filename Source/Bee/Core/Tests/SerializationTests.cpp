//
//  SerializationTests.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 7/05/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include <Bee/Core/Serialization/Serialization.hpp>
#include <Bee/Core/Serialization/JSONSerializer.hpp>
#include <Bee/Core/Serialization/MemorySerializer.hpp>
#include <Bee/Core/IO.hpp>

#include <gtest/gtest.h>
#include <rapidjson/filereadstream.h>

using namespace bee;

struct PrimitivesStruct
{
    static int current_version;

    int     intval { -1 };
    u32     uval { 0 };
    char    charval { 0 };
    bool    boolval { false };
    u8      ubyteval { 0 };
    i8      ibyteval { -1 };
    int     removed { -1 };
};

int PrimitivesStruct::current_version = 1;


BEE_SERIALIZE(PrimitivesStruct, PrimitivesStruct::current_version)
{
    BEE_ADD_FIELD(1, intval);
    BEE_ADD_FIELD(1, charval);
    BEE_ADD_FIELD(1, boolval);
    BEE_ADD_FIELD(1, uval);
    BEE_ADD_INTEGRITY_CHECK(2);
    BEE_ADD_FIELD(2, ubyteval);
    BEE_ADD_FIELD(2, ibyteval);
    BEE_ADD_INTEGRITY_CHECK(3);
    BEE_REMOVE_FIELD(2, 3, int, removed, 123);
}

struct TestSerializer : public Serializer
{
    i32                 cursor { 0 };
    DynamicArray<u8>    buffer;

    bool begin()
    {
        cursor = 0;
        if (mode() == SerializerMode::writing)
        {
            buffer.clear();
        }
        return true;
    }

    void end()
    {
        // no-op
    }

    void convert_begin_type(const char* type_name) {}

    void convert_end_type() {}

    template <typename T>
    void convert_cbuffer(T* array, const i32 size, const char* name)
    {

    }

    template <typename T>
    void convert(T* value, const char* name)
    {
        if (mode() == SerializerMode::reading)
        {
            memcpy(value, buffer.data() + cursor, sizeof(T));
        }
        else
        {
            io::write(&buffer, *value);
        }

        cursor += sizeof(T);
    }
};


TEST(SerializationTests, primitives)
{
    PrimitivesStruct test_struct{};
    test_struct.intval = 23;
    test_struct.charval = 'j';
    test_struct.boolval = false;
    test_struct.uval = 100;
    test_struct.ubyteval = 100;
    test_struct.ibyteval = 64;

    // Test writing to buffer
    TestSerializer serializer{};
    serialize(SerializerMode::writing, &serializer, &test_struct);

    ASSERT_EQ(serializer.buffer.size(), 14);

    auto buffer_ptr = serializer.buffer.begin();
    ASSERT_EQ(*reinterpret_cast<int*>(buffer_ptr), serializer.version);
    buffer_ptr += sizeof(i32);
    ASSERT_EQ(*reinterpret_cast<int*>(buffer_ptr), test_struct.intval);
    buffer_ptr += sizeof(int);
    ASSERT_EQ(*reinterpret_cast<char*>(buffer_ptr), test_struct.charval);
    buffer_ptr += sizeof(char);
    ASSERT_EQ(*reinterpret_cast<bool*>(buffer_ptr), test_struct.boolval);
    buffer_ptr += sizeof(bool);
    ASSERT_EQ(*reinterpret_cast<u32*>(buffer_ptr), test_struct.uval);

    // Test reading from written buffer into a struct
    PrimitivesStruct read_struct{};
    serialize(SerializerMode::reading, &serializer, &read_struct);
    ASSERT_EQ(read_struct.intval, test_struct.intval);
    ASSERT_EQ(read_struct.charval, test_struct.charval);
    ASSERT_EQ(read_struct.boolval, test_struct.boolval);
    ASSERT_EQ(read_struct.uval, test_struct.uval);

    // Write out the test struct again but this time with a different revision
    PrimitivesStruct::current_version = 2;
    ASSERT_NO_FATAL_FAILURE(serialize(SerializerMode::writing, &serializer, &test_struct));

    // Version 2 contains a removed field - test to check that the default value is written out to the buffer
    ASSERT_EQ(*reinterpret_cast<int*>(serializer.buffer.data() + 20), 123);

    // Read the revision back into the read struct and test all values
    ASSERT_NO_FATAL_FAILURE(serialize(SerializerMode::reading, &serializer, &read_struct));
    ASSERT_EQ(read_struct.intval, test_struct.intval);
    ASSERT_EQ(read_struct.charval, test_struct.charval);
    ASSERT_EQ(read_struct.boolval, test_struct.boolval);
    ASSERT_EQ(read_struct.uval, test_struct.uval);
    ASSERT_EQ(read_struct.ubyteval, test_struct.ubyteval);
    ASSERT_EQ(read_struct.ibyteval, test_struct.ibyteval);

    // Read the removed value before it's been removed and replaced with a default
    ASSERT_NO_FATAL_FAILURE(serialize(SerializerMode::reading, &serializer, &read_struct));
    ASSERT_EQ(read_struct.removed, -1);
}

struct Id
{
    u32 value { 0 };
};

BEE_SERIALIZE(Id, 1)
{
    BEE_ADD_FIELD(1, value);
}

struct Settings
{
    bool is_active { false };

    struct NestedType
    {
        Id id_values[5] = { { 0 }, { 1 }, { 2 }, { 3 }, { 4 } };
    };

    NestedType nested;
};

BEE_SERIALIZE(Settings::NestedType, 1)
{
    BEE_ADD_FIELD(1, id_values);
}

BEE_SERIALIZE(Settings, 1)
{
    BEE_ADD_FIELD(1, is_active);
    BEE_ADD_FIELD(1, nested);
}

struct TestStruct
{
    int value { 0 };
    Settings settings;
};

BEE_SERIALIZE(TestStruct, 1)
{
    BEE_ADD_FIELD(1, value);
    BEE_ADD_FIELD(1, settings);
}

TEST(SerializationTests, complex_type)
{
    char json_buffer[] = R"({
    "TestStruct": {
        "bee::version": 1,
        "value": 25,
        "Settings": {
            "bee::version": 1,
            "is_active": true,
            "Settings::NestedType": {
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
    bee::JSONReader serializer(&json_str);
    TestStruct test{};
    serialize(SerializerMode::reading, &serializer, &test);
    ASSERT_EQ(test.value, 25);
    ASSERT_TRUE(test.settings.is_active);
    for (u32 i = 0; i < 5; ++i)
    {
        ASSERT_EQ(test.settings.nested.id_values[i].value, i);
    }

    bee::JSONWriter writer_ {};
    serialize(SerializerMode::writing, &writer_, &test);
    ASSERT_STREQ(writer_.c_str(), json_buffer);
}

template <typename T>
void assert_serialized_data(u8* data_begin, const T& expected, i32* serialized_size)
{
    ASSERT_EQ(*reinterpret_cast<T*>(data_begin), expected);
    *serialized_size = sizeof(T);
}

void assert_serialized_data(u8* data_begin, const Path& expected, i32* serialized_size)
{
    // size
    ASSERT_EQ(*reinterpret_cast<i32*>(data_begin), expected.size());
    // data
    const auto data_ptr = reinterpret_cast<char*>(data_begin + sizeof(i32));
    const auto serialized_string = StringView(data_ptr, expected.size());
    ASSERT_EQ(serialized_string, expected.view());
    *serialized_size = sizeof(i32) + serialized_string.size();
}

void assert_serialized_data(u8* data_begin, const String& expected, i32* serialized_size)
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
void assert_serialized_data(u8* data_begin, const Array<T, Mode>& expected, i32* serialized_size)
{
    // size
    ASSERT_EQ(*reinterpret_cast<i32*>(data_begin), expected.size());

    // data
    const auto data_ptr = data_begin + sizeof(i32);
    i32 cursor = 0;
    for (int elem_idx = 0; elem_idx < expected.size(); ++elem_idx)
    {
        i32 element_size = 0;
        assert_serialized_data(data_ptr + cursor, expected[elem_idx], &element_size);
        cursor += element_size;
    }

    *serialized_size = sizeof(i32) + cursor;
}

TEST(SerializationTests, core_types)
{
    bee::MemorySerializer::buffer_t buffer;
    bee::MemorySerializer serializer(&buffer);

    String str = "Jacob";
    DynamicArray<String> string_array = { "Jacob", "Is", "Cool" };
    FixedArray<FixedArray<int>> int_2d = { { 1, 2, 3}, { 4, 5, 6 }, { 7, 8, 9 } };

    // Test String for read/write
    serialize(SerializerMode::writing, &serializer, &str);
    i32 serialized_size = 0;
    assert_serialized_data(buffer.data(), str, &serialized_size);

    String deserialized_string;
    serialize(SerializerMode::reading, &serializer, &deserialized_string);

    ASSERT_EQ(deserialized_string, str);

    // Test dynamic array of strings for read/write
    serialize(SerializerMode::writing, &serializer, &string_array);
    assert_serialized_data(buffer.begin(), string_array, &serialized_size);

    DynamicArray<String> deserialized_string_array;
    serialize(SerializerMode::reading, &serializer, &deserialized_string_array);

    ASSERT_EQ(deserialized_string_array.size(), string_array.size());
    for (int str_idx = 0; str_idx < string_array.size(); ++str_idx)
    {
        ASSERT_EQ(deserialized_string_array[str_idx], string_array[str_idx]);
    }

    // Test multi-dimensional fixed array of fixed arrays of ints for read/write
    serialize(SerializerMode::writing, &serializer, &int_2d);
    assert_serialized_data(buffer.begin(), int_2d, &serialized_size);

    FixedArray<FixedArray<int>> deserialized_int_2d;
    serialize(SerializerMode::reading, &serializer, &deserialized_int_2d);
    ASSERT_EQ(deserialized_int_2d.size(), int_2d.size());
    for (int array_idx = 0; array_idx < int_2d.size(); ++array_idx)
    {
        ASSERT_EQ(deserialized_int_2d[array_idx].size(), int_2d[array_idx].size());
        for (int int_idx = 0; int_idx < int_2d[array_idx].size(); ++int_idx)
        {
            ASSERT_EQ(deserialized_int_2d[array_idx][int_idx], int_2d[array_idx][int_idx]);
        }
    }

    // Test paths
    auto test_path = Path::executable_path();
    serialize(SerializerMode::writing, &serializer, &test_path);
    assert_serialized_data(buffer.begin(), test_path, &serialized_size);

    // Test hashmaps
    DynamicHashMap<String, int> expected_map;
    expected_map.insert("one", 1);
    expected_map.insert("two", 2);
    expected_map.insert("three", 3);
    expected_map.insert("four", 4);
    expected_map.insert("five", 5);
    serialize(SerializerMode::writing, &serializer, &expected_map);

    DynamicHashMap<String, int> actual_map;
    serialize(SerializerMode::reading, &serializer, &actual_map);

    ASSERT_EQ(actual_map.size(), expected_map.size());
    for (auto& val : expected_map)
    {
        const auto found = actual_map.find(val.key);
        ASSERT_NE(found, nullptr);
        ASSERT_EQ(val.key, found->key);
        ASSERT_EQ(val.value, found->value);
    }
}
