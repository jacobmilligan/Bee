/*
 *  RecordFinder.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Storage.hpp"

#include <clang/ASTMatchers/ASTMatchFinder.h>


namespace bee {
namespace reflect {


/*
 *************************
 *
 * Custom diagnostics
 *
 *************************
 */
struct Diagnostics
{
    clang::DiagnosticsEngine*   engine;

    // Errors
    unsigned int    err_attribute_missing_equals { 0 };
    unsigned int    err_invalid_annotation_format { 0 };
    unsigned int    err_missing_version_added { 0 };
    unsigned int    err_parent_not_marked_for_serialization { 0 };
    unsigned int    err_field_not_marked_for_serialization { 0 };
    unsigned int    err_invalid_attribute_name_format { 0 };
    unsigned int    err_requires_explicit_ordering { 0 };
    unsigned int    err_id_is_not_unique { 0 };

    // Warnings
    unsigned int    warn_unknown_field_type { 0 };

    void init(clang::DiagnosticsEngine* diag_engine);

    clang::DiagnosticBuilder Report(clang::SourceLocation location, unsigned diag_id) const;
};


/*
 *************************
 *
 * Attribute parsing
 *
 *************************
 */
struct SerializationInfo
{
    bool                serializable { false };
    bool                using_explicit_versioning { false };
    i32                 serialized_version { 0 };
    i32                 version_added { 0 };
    i32                 version_removed { limits::max<i32>() };
    i32                 id { -1 };
    SerializationFlags  flags { SerializationFlags::packed_format }; // packed is implicit - table is explicitly requested
    const char*         serializer_function { nullptr };
};

struct AttributeParser
{
    Allocator*              allocator { nullptr };
    bool                    empty { false };
    bool                    is_field{ false };
    const char*             current { nullptr };
    const char*             end { nullptr };
    Diagnostics*            diagnostics { nullptr };
    clang::SourceLocation   location;

    bool init(const clang::Decl& decl, Diagnostics* new_diagnostics, Allocator* new_allocator);

    void next();

    bool advance_on_char(char c);

    bool is_value_end() const;

    void skip_whitespace();

    bool parse(DynamicArray<AttributeStorage>* dst_attributes, SerializationInfo* serialization_info);

    bool parse_attribute(DynamicArray<AttributeStorage>* dst_attributes, SerializationInfo* serialization_info);

    bool parse_name(String* dst);

    bool parse_value(AttributeStorage* attribute);

    bool parse_string(AttributeStorage* attribute);

    bool parse_symbol(AttributeStorage* attribute);

    bool parse_number(AttributeStorage* attribute);
};

/*
 *************************
 *
 * Record parent info
 *
 *************************
 */
struct FieldStorage
{
    i32                             order { -1 };
    clang::SourceLocation           location;
    Field                           value;
    const TypeInfo*                 type { nullptr };
    String                          name;
    String                          specialized_type;
    DynamicArray<AttributeStorage>  attributes;
    DynamicArray<ReflTypeRef>       template_args;

    explicit FieldStorage(Allocator* allocator)
        : name(allocator),
          specialized_type(allocator),
          attributes(allocator),
          template_args(allocator)
    {}
};

inline bool operator==(const FieldStorage& lhs, const FieldStorage& rhs)
{
    return lhs.order == rhs.order;
}

inline bool operator!=(const FieldStorage& lhs, const FieldStorage& rhs)
{
    return lhs.order != rhs.order;
}

inline bool operator>(const FieldStorage& lhs, const FieldStorage& rhs)
{
    return lhs.order > rhs.order;
}

inline bool operator<(const FieldStorage& lhs, const FieldStorage& rhs)
{
    return lhs.order < rhs.order;
}

inline bool operator>=(const FieldStorage& lhs, const FieldStorage& rhs)
{
    return lhs.order >= rhs.order;
}

inline bool operator<=(const FieldStorage& lhs, const FieldStorage& rhs)
{
    return lhs.order <= rhs.order;
}

struct ParentTypeContainer
{
    bool                                has_explicit_version { false };
    TypeBufferWriter<RecordTypeInfo>*   writer { nullptr };
    Span<const TemplateParameter>       template_parameters;
    DynamicArray<TypeFixup>             functions;
    DynamicArray<TypeFixup>             enums;
    DynamicArray<TypeFixup>             records;
    DynamicArray<FieldStorage>          fields;

    explicit ParentTypeContainer(TypeBufferWriter<RecordTypeInfo>* new_writer, Allocator* allocator)
        : writer(new_writer),
          functions(allocator),
          enums(allocator),
          records(allocator),
          fields(allocator)
    {}

    void add_record(TypeBuffer* buffer)
    {
        records.push_back(TypeFixup { buffer->index });
    }

    void add_function(TypeBuffer* buffer)
    {
        functions.push_back(TypeFixup { buffer->index });
    }

    void add_enum(TypeBuffer* buffer)
    {
        enums.push_back(TypeFixup { buffer->index });
    }

    void add_field(FieldStorage&& field)
    {
        fields.emplace_back(std::move(field));
    }
};

/*
 *************************
 *
 * ASTMatcher
 *
 *************************
 */
struct FieldCreateInfo
{
    llvm::StringRef                 name;
    i32                             index { -1 };
    clang::QualType                 qual_type;
    clang::SourceLocation           location;
    const clang::ASTContext*        ast_context { nullptr };
    const clang::ASTRecordLayout*   enclosing_layout { nullptr };
    const ParentTypeContainer*      parent { nullptr };
};

struct ASTMatcher final : public clang::ast_matchers::MatchFinder::MatchCallback
{
    TypeMap*                  type_map {nullptr };
    Diagnostics               diagnostics;
    llvm::SmallString<1024>   type_name;

    explicit ASTMatcher(TypeMap* type_map_to_use);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    llvm::StringRef print_name(const clang::NamedDecl& decl);

    void print_qualtype_name(String* dst, const clang::QualType& type, const clang::ASTContext& ast_context);

    void reflect_record(const clang::CXXRecordDecl& decl, ParentTypeContainer* parent = nullptr);

    void reflect_record_children(const clang::CXXRecordDecl& decl, ParentTypeContainer* storage);

    void reflect_enum(const clang::EnumDecl& decl, ParentTypeContainer* parent = nullptr);

    void reflect_field(const clang::FieldDecl& decl, const clang::ASTRecordLayout& enclosing_layout, ParentTypeContainer* parent);

    void reflect_function(const clang::FunctionDecl& decl, ParentTypeContainer* parent = nullptr);

    void reflect_array(const clang::FieldDecl& decl, ParentTypeContainer* parent, const clang::QualType& qualtype, AttributeParser* attr_parser);

    FieldStorage create_field(const FieldCreateInfo& info, Allocator* allocator);
};


} // namespace reflect
} // namespace bee