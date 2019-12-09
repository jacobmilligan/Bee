/*
 *  CodeGen.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/ReflectionV2.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

#include <time.h>

namespace bee {
namespace reflect {


static constexpr unsigned char bee_reflect_magic[] = { 0x7C, 0xDD, 0x93, 0xB4 };
static constexpr i32 bee_reflect_magic_size = sizeof(unsigned char) * static_array_length(bee_reflect_magic);


struct ReflectedFile;


enum class RegistrationVersion
{
    unknown = 0,
    init,
    current = init
};

/*
 * .registration files are defined in memory as:
 *
 * magic | type_count | hashes_offset | get_type_offset | hash_0,offset_0 | hash_1,offset1 | get_type_0 | get_type_1 | ...
 *
 * A registration header contains the size and hash for a single type in the file
 */
struct RegistrationHeader
{
    unsigned char       magic[8];
    RegistrationVersion version { RegistrationVersion::unknown };
    i32                 type_count { 0 };
    u32                 source_location_offset { 0 };
    u32                 source_location_size { 0 };
    u32                 hashes_offset { 0 };
    u32                 types_offset { 0 };
    u32                 types_byte_count { 0 };
};


struct RegistrationTypeOffset
{
    u32 hash { 0 };
    u32 offset { 0 };
};


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



void pretty_print_types(const Span<const Type*>& types, io::StringStream* stream);

void generate_reflection(const ReflectedFile& file, io::StringStream* stream);

void generate_registration(const Path& source_location, const Span<const Type*>& types, io::StringStream* stream);

void link_registrations(const Span<const Path>& search_paths, io::StringStream* stream);


} // namespace reflect
} // namespace bee