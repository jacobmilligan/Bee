/*
 *  SoATests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Containers/SoA.hpp>

#include <GTest.hpp>

struct TestStruct
{
    static int value;

    TestStruct()
    {
        ++value;
    }

    TestStruct(const TestStruct& other)
    {
        ++value;
    }

    TestStruct& operator=(const TestStruct& other)
    {
        ++value;
        return *this;
    }

    ~TestStruct()
    {
        --value;
    }
};

int TestStruct::value = 0;


void assert_addresses(void* lhs, void* rhs)
{
    ASSERT_EQ(lhs, rhs);
}

#define ASSERT_ADDRESSES_EQ(lhs, rhs)                                                       \
    ASSERT_TRUE(reinterpret_cast<const void*>(lhs) == reinterpret_cast<const void*>(rhs))   \
    << std::hex << (uintptr_t)lhs << "\n" << (uintptr_t)rhs

TEST(SoATests, get_arrays)
{
    bee::SoA<int, char, float> soa(1024);

    ASSERT_ADDRESSES_EQ(soa.get<0>(), soa.data());
    ASSERT_ADDRESSES_EQ(soa.get<1>(), soa.data() + sizeof(int) * 1024);
    ASSERT_ADDRESSES_EQ(soa.get<2>(), soa.data() + sizeof(int) * 1024 + sizeof(char) * 1024);

    ASSERT_EQ(soa.get<int>(), soa.get<0>());
    ASSERT_EQ(soa.get<char>(), soa.get<1>());
    ASSERT_EQ(soa.get<float>(), soa.get<2>());

    ASSERT_EQ(soa.size(), 0);
    ASSERT_EQ(soa.capacity(), 1024);
}

TEST(SoATests, push_and_pop)
{
    TestStruct::value = 0;

    bee::SoA<int, char, TestStruct> soa(1024);
    soa.push_back(1, 'k', TestStruct{});

    ASSERT_EQ(soa.get<int>()[0], 1);
    ASSERT_EQ(soa.get<char>()[0], 'k');
    ASSERT_EQ(soa.size(), 1);
    ASSERT_EQ(TestStruct::value, 1);

    soa.pop_back();

    ASSERT_EQ(soa.size(), 0);
    ASSERT_EQ(TestStruct::value, 0);

    for (int i = 0; i < soa.capacity(); ++i)
    {
        soa.push_back(i, static_cast<char>(i), TestStruct{});
    }

    ASSERT_EQ(TestStruct::value, 1024);

    for (int i = 0; i < soa.size(); ++i)
    {
        ASSERT_EQ(soa.get<int>()[i], i);
        ASSERT_EQ(soa.get<char>()[i], static_cast<char>(i));
    }

    while (!soa.empty())
    {
        soa.pop_back();
    }

    ASSERT_EQ(TestStruct::value, 0);

    for (int i = 0; i < soa.capacity(); ++i)
    {
        soa.push_back(i, static_cast<char>(i), TestStruct{});
    }
    ASSERT_EQ(TestStruct::value, 1024);
    soa.clear();
    ASSERT_EQ(TestStruct::value, 0);
}
