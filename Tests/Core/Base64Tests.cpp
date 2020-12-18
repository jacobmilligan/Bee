/*
 *  Base64.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Base64.hpp>

#include <gtest/gtest.h>

static const char* encode_string(const bee::StringView& string)
{
    static char buffer[4096];
    const int encoded_size = bee::base64_encode(
        buffer,
        bee::static_array_length(buffer),
        reinterpret_cast<const bee::u8*>(string.data()),
        string.size()
    );
    buffer[encoded_size] = '\0';
    return buffer;
}

static const char* decode_string(const bee::StringView& string)
{
    static char buffer[4096];
    const int decoded_size = bee::base64_decode(reinterpret_cast<bee::u8*>(buffer), bee::static_array_length(buffer), string);
    buffer[decoded_size] = '\0';
    return buffer;
}

TEST(Base64Tests, encode)
{
    ASSERT_STREQ(encode_string("") , "");
    ASSERT_STREQ(encode_string("f"), "Zg==");
    ASSERT_STREQ(encode_string("fo"), "Zm8=");
    ASSERT_STREQ(encode_string("foo"), "Zm9v");
    ASSERT_STREQ(encode_string("foob"), "Zm9vYg==");
    ASSERT_STREQ(encode_string("fooba"), "Zm9vYmE=");
    ASSERT_STREQ(encode_string("foobar"), "Zm9vYmFy");
    ASSERT_STREQ(encode_string("Bee test case"), "QmVlIHRlc3QgY2FzZQ==");
    ASSERT_STREQ(
        encode_string(
            "Have you ever had a dream that you, um, "
            "you had, your, you- you could, you’ll do, "
            "you- you wants, you, you could do so, you- "
            "you’ll do, you could- you, you want, "
            "you want them to do you so much you could do anything?"
        ),
        "SGF2ZSB5b3UgZXZlciBoYWQgYSBkcmVhbSB0aGF0IHlvdSwgdW0sIHlvdSBo"
        "YWQsIHlvdXIsIHlvdS0geW91IGNvdWxkLCB5b3XigJlsbCBkbywgeW91LSB5"
        "b3Ugd2FudHMsIHlvdSwgeW91IGNvdWxkIGRvIHNvLCB5b3UtIHlvdeKAmWxs"
        "IGRvLCB5b3UgY291bGQtIHlvdSwgeW91IHdhbnQsIHlvdSB3YW50IHRoZW0gd"
        "G8gZG8geW91IHNvIG11Y2ggeW91IGNvdWxkIGRvIGFueXRoaW5nPw=="
    );
}

TEST(Base64Tests, decode)
{
    ASSERT_STREQ(decode_string(""), "");
    ASSERT_STREQ(decode_string("Zg=="), "f");
    ASSERT_STREQ(decode_string("Zm8="), "fo");
    ASSERT_STREQ(decode_string("Zm9v"), "foo");
    ASSERT_STREQ(decode_string("Zm9vYg=="), "foob");
    ASSERT_STREQ(decode_string("Zm9vYmE="), "fooba");
    ASSERT_STREQ(decode_string("Zm9vYmFy"), "foobar");
    ASSERT_STREQ(decode_string("QmVlIHRlc3QgY2FzZQ=="), "Bee test case");
    ASSERT_STREQ(
        decode_string(
            "SGF2ZSB5b3UgZXZlciBoYWQgYSBkcmVhbSB0aGF0IHlvdSwgdW0sIHlvdSBo"
            "YWQsIHlvdXIsIHlvdS0geW91IGNvdWxkLCB5b3XigJlsbCBkbywgeW91LSB5"
            "b3Ugd2FudHMsIHlvdSwgeW91IGNvdWxkIGRvIHNvLCB5b3UtIHlvdeKAmWxs"
            "IGRvLCB5b3UgY291bGQtIHlvdSwgeW91IHdhbnQsIHlvdSB3YW50IHRoZW0gd"
            "G8gZG8geW91IHNvIG11Y2ggeW91IGNvdWxkIGRvIGFueXRoaW5nPw=="
        ),
        "Have you ever had a dream that you, um, "
        "you had, your, you- you could, you’ll do, "
        "you- you wants, you, you could do so, you- "
        "you’ll do, you could- you, you want, "
        "you want them to do you so much you could do anything?"
    );
}