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


namespace bee {

class PathView;

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
    templates_only,
    none
};

class CodeGenerator
{
public:
    explicit CodeGenerator(const CodegenMode mode, io::StringStream* stream, const i32 indent_size = 4);

    void reset(io::StringStream* new_stream);

    i32 generated_count() const;

    i32 set_indent(const i32 indent);

    void indent();

    void newline(const i32 count = 1);

    bool should_generate(const TypeInfo& type);

    void write_header_comment(const PathView& source_location);

    void write_type_signature(const TypeInfo& type, const CodegenMode force_mode = CodegenMode::none);

    const char* as_ident(const TypeInfo& type);

    const char* as_ident(const StringView& name, const char* prefix = nullptr);

    void append_line(const char* format, ...) BEE_PRINTFLIKE(2, 3);

    void write(const char* format, ...) BEE_PRINTFLIKE(2, 3);

    void write_line(const char* format, ...) BEE_PRINTFLIKE(2, 3);

    void type_guard_begin(const TypeInfo* type);

    void type_guard_end(const TypeInfo* type);

    template <typename StorageType, typename CallbackType>
    void generate(StorageType* storage, CallbackType&& callback)
    {
        type_guard_begin(&storage->type);
        callback(this, storage);
        type_guard_end(&storage->type);
        ++generated_count_;
    }

    template <typename StorageType, typename CallbackType>
    void generate_no_guard(StorageType* storage, CallbackType&& callback)
    {
        callback(this, storage);
        ++generated_count_;
    }

    void serializer_function(const char* field_name, const char* specialized_type_name);

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
        scope(BEE_FORWARD(lambda));
        stream_->write(after);
    }

    inline CodegenMode mode() const
    {
        return mode_;
    }

    inline bool include_serialization_header() const
    {
        return include_serialization_header_;
    }
private:
    StaticString<4096>  ident_buffer_;
    io::StringStream*   stream_ { nullptr };
    CodegenMode         mode_ { CodegenMode::cpp };
    i32                 indent_size_ { 0 };
    i32                 indent_ { 0 };
    i32                 generated_count_ { 0 };
    bool                include_serialization_header_ { false };
    BEE_PAD(7);
};

struct TypeListEntry;

void generate_empty_reflection(const PathView& dst_path, const char* location, String* output);

i32 generate_reflection(const PathView& dst_path, const ReflectedFile& file, String* output, CodegenMode mode);

i32 generate_reflection_header(const PathView& dst_path, const ReflectedFile& file, const i32 first_type_index, String* output, CodegenMode mode);

void generate_typelist(const PathView& target_dir, const Span<const TypeListEntry>& all_types, CodegenMode mode, const Span<const PathView>& written_files);


} // namespace reflect
} // namespace bee