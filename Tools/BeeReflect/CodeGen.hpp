/*
 *  CodeGen.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/ReflectionV2.hpp"
#include "Bee/Core/IO.hpp"


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

const char* reflection_flag_to_string(const bee::Qualifier qualifier);

const char* reflection_flag_to_string(const bee::StorageClass storage_class);

const char* reflection_type_kind_to_string(const bee::TypeKind type_kind);

template <typename FlagType>
const char* reflection_dump_flags(const FlagType flag)
{
    static thread_local char buffer[4096];
    bee::io::StringStream stream(buffer, bee::static_array_length(buffer), 0);

    int count = 0;
    bee::for_each_flag(flag, [&](const FlagType& f)
    {
        stream.write_fmt(" %s |", reflection_flag_to_string(f));
        ++count;
    });

    if (count == 0)
    {
        stream.write(reflection_flag_to_string(static_cast<FlagType>(0u)));
    }

    if (buffer[stream.size() - 1] == '|')
    {
        buffer[stream.size() - 1] = '\0';
    }

    // Skip the first space that occurs when getting multiple flags
    return count > 0 ? buffer + 1 : buffer;
}


}