/*
 *  GUIDTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/GUID.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"

#include <gtest/gtest.h>

TEST(GUIDTests, guids_generate)
{
    auto guid = bee::generate_guid();

    auto string = bee::guid_to_string(guid, bee::GUIDFormat::digits);
    auto string_hyphens = bee::guid_to_string(guid, bee::GUIDFormat::digits_with_hyphen);
    auto string_braced = bee::guid_to_string(guid, bee::GUIDFormat::braced_digits_with_hyphen);
    auto string_parens = bee::guid_to_string(guid, bee::GUIDFormat::parens_digits_with_hyphen);

    auto guid2 = bee::guid_from_string(string.view());
    auto guid2_hyphens = bee::guid_from_string(string_hyphens.view());
    auto guid2_braced = bee::guid_from_string(string_braced.view());
    auto guid2_parens = bee::guid_from_string(string_parens.view());

    ASSERT_EQ(guid, guid2);
    ASSERT_EQ(guid, guid2_hyphens);
    ASSERT_EQ(guid, guid2_braced);
    ASSERT_EQ(guid, guid2_parens);

    auto string2 = bee::guid_to_string(guid2, bee::GUIDFormat::digits);
    auto string2_hyphens = bee::guid_to_string(guid2_hyphens, bee::GUIDFormat::digits_with_hyphen);
    auto string2_braced = bee::guid_to_string(guid2_braced, bee::GUIDFormat::braced_digits_with_hyphen);
    auto string2_parens = bee::guid_to_string(guid2_parens, bee::GUIDFormat::parens_digits_with_hyphen);

    ASSERT_STREQ(string.c_str(), string2.c_str());
    ASSERT_STREQ(string_hyphens.c_str(), string2_hyphens.c_str());
    ASSERT_STREQ(string_braced.c_str(), string2_braced.c_str());
    ASSERT_STREQ(string_parens.c_str(), string2_parens.c_str());
}

TEST(GUIDTests, invalid_guids)
{
    ASSERT_DEATH(bee::guid_from_string("asdasdasd"), "Check failed");
    ASSERT_DEATH(bee::guid_from_string("{00000000-0000-0000-0000-000000000000"), "Invalid GUID");
}

TEST(GUIDTests, source_buffer_for_string_conversion)
{
    auto guid = bee::generate_guid();

    auto string = bee::guid_to_string(guid, bee::GUIDFormat::digits);
    auto string_hyphens = bee::guid_to_string(guid, bee::GUIDFormat::digits_with_hyphen);
    auto string_braced = bee::guid_to_string(guid, bee::GUIDFormat::braced_digits_with_hyphen);
    auto string_parens = bee::guid_to_string(guid, bee::GUIDFormat::parens_digits_with_hyphen);

    char buffer[256]{0};
    const auto span = bee::make_span(buffer, 256);
    auto char_count = bee::guid_to_string(guid, bee::GUIDFormat::digits, span);
    ASSERT_EQ(char_count, string.size());

    char_count = bee::guid_to_string(guid, bee::GUIDFormat::digits_with_hyphen, span);
    ASSERT_EQ(char_count, string_hyphens.size());

    char_count = bee::guid_to_string(guid, bee::GUIDFormat::braced_digits_with_hyphen, span);
    ASSERT_EQ(char_count, string_braced.size());

    char_count = bee::guid_to_string(guid, bee::GUIDFormat::parens_digits_with_hyphen, span);
    ASSERT_EQ(char_count, string_parens.size());
}

TEST(GUIDTests, guid_serialization)
{
    auto guid = bee::generate_guid();
    auto string = bee::guid_to_string(guid, bee::GUIDFormat::digits); // for comparison
    char stringbuf[33];

    const auto offset = offsetof(bee::GUID, data);
    const auto guid_type = bee::get_type_as<bee::GUID, bee::RecordType>();
    ASSERT_EQ(offset, guid_type->fields[0].offset);

    bee::io::StringStream stream(stringbuf, 33, 0);
    bee::StreamSerializer serializer(&stream);
    bee::serialize(bee::SerializerMode::writing, &serializer, &guid);
    ASSERT_STREQ(stringbuf, string.c_str());

    bee::GUID read_guid{};
    bee::serialize(bee::SerializerMode::reading, &serializer, &read_guid);

    ASSERT_EQ(guid, read_guid);

    const auto read_string = bee::guid_to_string(read_guid, bee::GUIDFormat::digits);

    ASSERT_STREQ(string.c_str(), read_string.c_str());
}
