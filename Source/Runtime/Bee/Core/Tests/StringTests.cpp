/*
 *  StringTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/String.hpp>
#include <Bee/Core/Memory/MallocAllocator.hpp>
#include <Bee/Core/Memory/LinearAllocator.hpp>
#include <Bee/Core/IO.hpp>

#include <gtest/gtest.h>

TEST(StringTests, construct_copy_move)
{
    bee::MallocAllocator malloc_allocator;
    bee::LinearAllocator linear_allocator(bee::kibibytes(4));

    const char* raw_test_string = "Test string 1";

    // Test construction
    bee::String string_a(&malloc_allocator);
    bee::String string_b(10, 'x', &malloc_allocator);
    bee::String string_c(raw_test_string, &malloc_allocator);

    auto test_string_a = [](const bee::String& string)
    {
        ASSERT_STREQ(string.data(), "\0");
        ASSERT_EQ(string.size(), 0);
        ASSERT_EQ(string.capacity(), 0);
    };

    auto test_string_b = [](const bee::String& string, const char* char_sequence)
    {
        ASSERT_STREQ(string.c_str(), char_sequence);
        ASSERT_EQ(string.size(), 10);
        ASSERT_EQ(string.capacity(), 11);
    };

    auto test_string_c = [](const bee::String& string, const char* raw_string)
    {
        ASSERT_STREQ(string.c_str(), raw_string);
        ASSERT_EQ(string.size(), strlen(raw_string));
        ASSERT_EQ(string.capacity(), strlen(raw_string) + 1);
    };

    auto test_moved_from = [](const bee::String& string)
    {
        ASSERT_EQ(string.allocator(), nullptr);
        ASSERT_EQ(string.size(), 0);
        ASSERT_EQ(string.capacity(), 0);
    };

    test_string_a(string_a);
    test_string_b(string_b, "xxxxxxxxxx");
    test_string_c(string_c, raw_test_string);

    // Test copy
    auto copy_a = string_a;
    test_string_a(copy_a);
    ASSERT_EQ(copy_a.allocator(), &malloc_allocator);

    auto copy_b = string_b;
    test_string_b(copy_b, "xxxxxxxxxx");
    ASSERT_EQ(copy_b.allocator(), &malloc_allocator);

    auto copy_c = string_c;
    test_string_c(copy_c, raw_test_string);
    ASSERT_EQ(copy_c.allocator(), &malloc_allocator);

    // Test move
    auto move_a = std::move(string_a);
    test_string_a(move_a);
    ASSERT_EQ(move_a.allocator(), &malloc_allocator);
    test_moved_from(string_a);

    auto move_b = std::move(string_b);
    test_string_b(move_b, "xxxxxxxxxx");
    ASSERT_EQ(move_b.allocator(), &malloc_allocator);
    test_moved_from(string_b);

    auto move_c = std::move(string_c);
    test_string_c(move_c, raw_test_string);
    ASSERT_EQ(move_c.allocator(), &malloc_allocator);
    test_moved_from(string_c);

    // Test copy and move with different allocators
    const char* raw_allocator_string = "Allocator test 1";
    bee::String allocator_test_a(&linear_allocator);
    bee::String allocator_test_b(10, 'y', &linear_allocator);
    bee::String allocator_test_c(raw_allocator_string, &linear_allocator);

    test_string_a(allocator_test_a);
    test_string_b(allocator_test_b, "yyyyyyyyyy");
    test_string_c(allocator_test_c, raw_allocator_string);

    // Test destructive copy with different allocators
    copy_a = allocator_test_a;
    copy_b = allocator_test_b;
    copy_c = allocator_test_c;

    test_string_a(copy_a);
    ASSERT_EQ(copy_a.allocator(), &linear_allocator);

    test_string_b(copy_b, "yyyyyyyyyy");
    ASSERT_EQ(copy_b.allocator(), &linear_allocator);

    test_string_c(copy_c, raw_allocator_string);
    ASSERT_EQ(copy_c.allocator(), &linear_allocator);

    /*
     * Test memory allocated is as expected with the new allocator. Need to check each allocation was aligned correctly
     * by the allocator
     */
    bee::String* strings[] = { &allocator_test_a, &allocator_test_b, &allocator_test_c, &copy_a, &copy_b, &copy_c };
    int total_size = 0;
    int expected_size = 0;
    for (const auto& s : strings)
    {
        total_size += s->size();
        if (!s->empty())
        {
            expected_size = bee::round_up(expected_size + sizeof(size_t), sizeof(void*)) + s->size() + 1;
        }
    }

    ASSERT_EQ(linear_allocator.offset(), expected_size);

    allocator_test_a.~String();
    allocator_test_b.~String();
    allocator_test_c.~String();
    copy_a.~String();
    copy_b.~String();
    copy_c.~String();

    ASSERT_EQ(linear_allocator.offset(), expected_size);
}

TEST(StringTests, append)
{
    auto string = bee::String("Test string");
    ASSERT_STREQ(string.c_str(), "Test string");

    string += " + Test string 1";

    ASSERT_STREQ(string.c_str(), "Test string + Test string 1");

    bee::String string2(" + Test string 2");

    string = string + string2;

    ASSERT_STREQ(string.c_str(), "Test string + Test string 1 + Test string 2");

    // Test for range iteration
    auto cur_idx = 0;
    for (auto& c : string)
    {
        ASSERT_EQ(c, string[cur_idx]);
        ++cur_idx;
    }
    ASSERT_EQ(cur_idx, string.size()); // ensure the range was the whole length of the string
    ASSERT_EQ(string.back(), '2');
    ASSERT_EQ(string.size(), 43);

    bee::String char_string("");
    char_string += '.';
    char_string += ',';
    ASSERT_STREQ(char_string.c_str(), ".,");
}

TEST(StringTests, insert)
{
    bee::String string("Test");
    string.insert(string.size(), "Jacob");
    ASSERT_STREQ(string.c_str(), "TestJacob");

    string.insert(0, "wat");
    ASSERT_STREQ(string.c_str(), "watTestJacob");

    string.insert(3, " this ");
    ASSERT_STREQ(string.c_str(), "wat this TestJacob");

    string.insert(9, "is a ");
    ASSERT_STREQ(string.c_str(), "wat this is a TestJacob");
}

TEST(StringTests, remove)
{
    bee::String string("This is a test string for removing");
    string.remove(0, 5);
    ASSERT_STREQ(string.c_str(), "is a test string for removing");

    string.remove(20, 9);
    ASSERT_STREQ(string.c_str(), "is a test string for");

    string.remove(4, 5);
    ASSERT_STREQ(string.c_str(), "is a string for");

    string.remove(4, 7);
    ASSERT_STREQ(string.c_str(), "is a for");

    string.remove(2);
    ASSERT_STREQ(string.c_str(), "is");

    string.remove(0);
    ASSERT_TRUE(string.empty());
    ASSERT_EQ(string.size(), 0);
}

TEST(StringTests, format)
{
    auto formatted = bee::str::format("Test %s", "Jacob");
    ASSERT_STREQ(formatted.c_str(), "Test Jacob");

    // Test large raw strings
    const char* large_raw_str = R"(

This is to test if %s can format a large string with formatted size %d - %f, %llu

)";
    formatted = bee::str::format(large_raw_str, "Bee", strlen(large_raw_str), 1.0f, bee::u64(23));

    ASSERT_STREQ(formatted.c_str(), R"(

This is to test if Bee can format a large string with formatted size 85 - 1.000000, 23

)");

}

TEST(StringTests, last_and_first_index_of)
{
    bee::String string("A test string for substrings - finding the last occurrence of a character or substring in a bee::String");
    bee::String substring("substring");

    ASSERT_EQ(bee::str::last_index_of(string, 'g'), string.size() - 1);
    ASSERT_EQ(bee::str::last_index_of(string, "bee"), string.size() - 11);
    ASSERT_EQ(bee::str::last_index_of(string, substring), string.size() - 26);

    ASSERT_EQ(bee::str::first_index_of(string, 'g'), 12);
    ASSERT_EQ(bee::str::first_index_of(string, "bee"), string.size() - 11);
    ASSERT_EQ(bee::str::first_index_of(string, substring), 18);
}

TEST(StringTests, replace)
{
    bee::String string("This is a really cool test string");

    bee::str::replace(&string, 'c', 'w');
    ASSERT_STREQ(string.c_str(), "This is a really wool test string");

    bee::str::replace(&string, "really wool", "modified");
    ASSERT_STREQ(string.c_str(), "This is a modified test string");

    bee::str::replace(&string, " ", ".");
    ASSERT_STREQ(string.c_str(), "This.is.a.modified.test.string");

    bee::str::replace(&string, ".", "");
    ASSERT_STREQ(string.c_str(), "Thisisamodifiedteststring");

    string = "Replace range string";
    bee::str::replace_range(&string, 8, 5, "the string is larger");
    ASSERT_STREQ(string.c_str(), "Replace the string is larger string");

    bee::str::replace_range(&string, 12, 16, "smaller");
    ASSERT_STREQ(string.c_str(), "Replace the smaller string");
}

TEST(StringTests, substring)
{
    bee::String string("Test string for substring testing");
    const auto substr = bee::str::substring(string, 16, 9);
    ASSERT_EQ(substr, "substring");

    const auto substr_substr = bee::str::substring(substr, 3);
    ASSERT_EQ(substr_substr, "string");

    ASSERT_STREQ(string.c_str(), "Test string for substring testing");

    bee::String new_string(substr);
    ASSERT_STREQ(new_string.c_str(), "substring");
}

TEST(StringTests, format_and_write)
{
    auto string = bee::str::format("Hello my name is %s", "Jacob");
    ASSERT_STREQ(string.c_str(), "Hello my name is Jacob");

    string = bee::str::format("%d, %d, %d, %s, %#04x", 1, 2, 3, "4", 5);
    ASSERT_STREQ(string.c_str(), "1, 2, 3, 4, 0x05");

    string.clear();
    bee::io::write_fmt(&string, "%s, %d, %#04x, %c", "Test", 1, 2, 'x');
    ASSERT_STREQ(string.c_str(), "Test, 1, 0x02, x");


    bee::String byte_string;
    int cur_byte_idx = 0;
    bee::u8 bytes[] = { 2, 5, 7, 1, 0 };
    for (auto& byte : bytes) {
        bee::io::write_fmt(&byte_string, "%#04x", byte);
        if (cur_byte_idx < bee::static_array_length(bytes) - 1) {
            byte_string += bee::String(", ");
        }
        ++cur_byte_idx;
    }

    ASSERT_STREQ(byte_string.c_str(), "0x02, 0x05, 0x07, 0x01, 0000");
}
