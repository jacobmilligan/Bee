/*
 *  IOTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/IO.hpp>

#include <gtest/gtest.h>


// This is a copy-paste of the internal error handling code in Error.cpp - it's a case that the stream seems to break
// on frequently due to having to process lots of write_fmt and write_v
bee::String log_assert_message(
    const char* assert_msg,
    const char* function,
    const char* file,
    const int line,
    const char* expr,
    const char* user_fmt,
    va_list user_args
)
{
    static constexpr auto error_line_start = "          ";

    bee::String msg_string;
    bee::io::StringStream msg_stream(&msg_string);

    msg_stream.write_fmt("Skyrocket => %s", assert_msg);

    if (expr != nullptr)
    {
        msg_stream.write_fmt(" (%s)", expr);

    }

    msg_stream.write_fmt("\n%s", "   ");
    msg_stream.write_fmt("=> at %s:%d\n%s   in function %s", file, line, error_line_start, function);

    if (user_fmt != nullptr)
    {
        msg_stream.write_fmt("\n%s=> reason: ", error_line_start);
        msg_stream.write_v(user_fmt, user_args);
    }

    return msg_string;
}


bee::String test_passing_va_list(
    int* line_out,
    const char* function,
    const char* file,
    const int line,
    const char* expr,
    const char* msgformat,
    ...
)
{
    *line_out = line;
    va_list args;
    va_start(args, msgformat);
    const auto msg = log_assert_message("Check failed", function, file, line, expr, msgformat, args);
    va_end(args);
    return msg;
}

#define BEE_TEST_PASSING_VA_LIST(line_ptr, expr, msg, ...) test_passing_va_list(line_ptr, BEE_FUNCTION_NAME, __FILE__, __LINE__, #expr, msg, ##__VA_ARGS__)


TEST(IOTests, stringstream)
{
    // Test read only strings
    const char* read_only_string = "This is a test to ensure streams for c-style string work";
    bee::io::StringStream stream(read_only_string, bee::str::length(read_only_string));

    ASSERT_STREQ(stream.c_str(), read_only_string);

    // Test reading into a string buffer from a string
    char test_buffer[1024] { '\0' };
    stream.read(test_buffer, bee::str::length(read_only_string));

    ASSERT_STREQ(test_buffer, read_only_string);

    // Test read write string buffers
    new (&stream) bee::io::StringStream(test_buffer, 1024, 0);

    stream.write("StringView write ");
    const char* append = "const char* write";
    stream.write(append, bee::str::length(append));
    ASSERT_STREQ(test_buffer, "StringView write const char* write"); // ensure null terminator

    // Test reading from the read-write buffer into another buffer
    char another_buffer[1024] { '\0' };
    const auto read_size = stream.offset();
    stream.seek(0, bee::io::SeekOrigin::begin);
    stream.read(another_buffer, read_size);
    ASSERT_STREQ(another_buffer, "StringView write const char* write");

    const auto full_erased_str = "StringView write const char* write this should be erased";

    stream.write(" this should be erased");
    ASSERT_STREQ(stream.c_str(), full_erased_str);

    const auto erased_size = stream.size();

    // Test string containers
    bee::String rw_string;
    new (&stream) bee::io::StringStream(&rw_string);

    stream.write("Testing StringView ");
    stream.write(full_erased_str, erased_size);
    stream.write(" ");
    stream.write(read_only_string, bee::str::length(read_only_string));

    const auto string_container_test_str = "Testing StringView StringView write const char* write this should be "
                                           "erased This is a test to ensure streams for c-style string work";
    ASSERT_STREQ(stream.c_str(), string_container_test_str);

    // Test reading whole stream into a string
    stream.seek(0, bee::io::SeekOrigin::begin);
    bee::String read_string;
    stream.read(&read_string);

    ASSERT_STREQ(stream.c_str(), read_string.c_str());

    // Test reading one char at a time
    read_string.clear();
    stream.seek(0, bee::io::SeekOrigin::begin);
    for (int i = 0; i < stream.size(); ++i)
    {
        stream.read(&read_string, i, 1);
    }
    ASSERT_STREQ(stream.c_str(), read_string.c_str());
    ASSERT_STREQ(string_container_test_str, read_string.c_str());

    // Test writing format strings
    stream.seek(0, bee::io::SeekOrigin::begin);
    stream.write_fmt("%s, %d, %f", "Test", 30, 1.0f);
    ASSERT_STREQ(stream.c_str(), "Test, 30, 1.000000");

    int line = 0;
    auto msg = BEE_TEST_PASSING_VA_LIST(&line, 25 == 50, "This works! %s %d", "Another test", 1);
    bee::String expected = R"(Skyrocket => Check failed (25 == 50)
   => at )" __FILE__ ":"
   + bee::str::to_string(line)
   + R"(
             in function void __cdecl IOTests_stringstream_Test::TestBody(void)
          => reason: This works! Another test 1)";
    ASSERT_STREQ(msg.c_str(), expected.c_str());
}
