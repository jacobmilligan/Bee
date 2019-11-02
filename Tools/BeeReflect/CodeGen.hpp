/*
 *  CodeGen.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/ReflectionV2.hpp"
#include "Bee/Core/IO.hpp"


#include <time.h>


namespace bee {


class CodeGenerator
{
public:
    explicit CodeGenerator(io::StringStream* stream, const i32 indent_size = 4)
        : stream_(stream),
          indent_size_(indent_size)
    {}

    void indent()
    {
        for (int i = 0; i < indent_; ++i)
        {
            stream_->write(' ');
        }
    }

    void newline()
    {
        stream_->write("\n");
    }

    void write_header_comment(const char* source_location)
    {
        char time_string[256];
        const time_t timepoint = ::time(nullptr);
        const auto timeinfo = localtime(&timepoint);
        ::strftime(time_string, static_array_length(time_string), BEE_TIMESTAMP_FMT, timeinfo);

        write_line(R"(/*
 *  This file was generated by the bee-reflect tool. DO NOT EDIT DIRECTLY.
 *
 *  Generated on: %s
 *  Source: %s
 */
)", time_string, source_location);
    }

    void write_header_comment(const Path& source_location)
    {
        write_header_comment(source_location.c_str());
    }

    void append_line(const char* format, ...) BEE_PRINTFLIKE(2, 3)
    {
        va_list args;
        va_start(args, format);

        stream_->write_v(format, args);

        va_end(args);
    }


    void write(const char* format, ...) BEE_PRINTFLIKE(2, 3)
    {
        va_list args;
        va_start(args, format);

        indent();
        stream_->write_v(format, args);

        va_end(args);
    }

    void write_line(const char* format, ...) BEE_PRINTFLIKE(2, 3)
    {
        va_list args;
        va_start(args, format);

        indent();
        stream_->write_v(format, args);
        stream_->write("\n");

        va_end(args);
    }

    template <typename LambdaType>
    void scope(LambdaType&& lambda)
    {
        stream_->write("\n");
        indent();
        stream_->write("{\n");

        indent_ += indent_size_;
        lambda();
        indent_ -= indent_size_;

        stream_->write("\n");
        indent();
        stream_->write("}");
    }

    template <typename LambdaType>
    void scope(LambdaType&& lambda, const char* after)
    {
        scope(std::forward<LambdaType>(lambda));
        stream_->write(after);
    }
private:
    io::StringStream* stream_ { nullptr };
    i32               indent_size_ { 0 };
    i32               indent_ { 0 };
};



void reflection_pretty_print(const Span<const Type*>& types, io::StringStream* stream);

void reflection_codegen(const Path& source_location, const Span<const Type*>& types, io::StringStream* stream);


}