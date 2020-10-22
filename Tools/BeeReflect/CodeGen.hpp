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

class CodeGenerator
{
public:
    explicit CodeGenerator(const CodegenMode mode, io::StringStream* stream, const i32 indent_size = 4);

    void reset(io::StringStream* new_stream);

    i32 set_indent(const i32 indent);

    void indent();

    void newline();

    bool should_generate(const TypeInfo& type);

    void write_header_comment(const char* source_location);

    void write_header_comment(const Path& source_location);

    void write_type_signature(const TypeInfo& type);

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

void generate_typelist(const Path& target_dir, const Span<const TypeInfo*>& all_types, CodegenMode mode, const Span<const Path>& written_files);

void link_typelists(const Path& output_path, const Span<const Path>& search_paths);


} // namespace reflect
} // namespace bee