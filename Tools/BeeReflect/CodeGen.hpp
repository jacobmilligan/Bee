/*
 *  CodeGen.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

#include <time.h>
#include <llvm/ADT/ArrayRef.h>

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

enum class CodegenMode
{
    cpp,
    inl,
    templates_only
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
    explicit CodeGenerator(const CodegenMode mode, io::StringStream* stream, const i32 indent_size = 4);

    void reset(io::StringStream* new_stream);

    i32 set_indent(const i32 indent);

    void indent();

    void newline();

    bool should_generate(const Type& type);

    void write_header_comment(const char* source_location);

    void write_header_comment(const Path& source_location);

    void write_type_signature(const Type& type);

    void append_line(const char* format, ...) BEE_PRINTFLIKE(2, 3);

    void write(const char* format, ...) BEE_PRINTFLIKE(2, 3);

    void write_line(const char* format, ...) BEE_PRINTFLIKE(2, 3);

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

    inline CodegenMode mode() const
    {
        return mode_;
    }
private:
    CodegenMode         mode_ { CodegenMode::cpp };
    io::StringStream*   stream_ { nullptr };
    i32                 indent_size_ { 0 };
    i32                 indent_ { 0 };
};

void pretty_print_types(const Span<const Type*>& types, io::StringStream* stream);

void generate_empty_reflection(const char* location, io::StringStream* stream);

i32 generate_reflection(const ReflectedFile& file, io::StringStream* stream, CodegenMode mode);

void generate_typelist(const Path& target_dir, const Span<const Type*>& all_types);

void link_typelists(const Path& output_path, const Span<const Path>& search_paths);


} // namespace reflect
} // namespace bee