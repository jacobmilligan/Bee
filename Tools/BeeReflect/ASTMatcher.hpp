/*
 *  RecordFinder.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Storage.hpp"

BEE_PUSH_WARNING
    BEE_DISABLE_PADDING_WARNINGS
    BEE_DISABLE_WARNING_MSVC(4996)
    #include <clang/ASTMatchers/ASTMatchFinder.h>
BEE_POP_WARNING


namespace bee {
namespace reflect {


class ReflectionAllocator;

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
    BEE_PAD(4);

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
    BEE_PAD(2);
    i32                 serialized_version { 0 };
    i32                 version_added { 0 };
    i32                 version_removed { limits::max<i32>() };
    i32                 id { -1 };
    SerializationFlags  flags { SerializationFlags::packed_format }; // packed is implicit - table is explicitly requested
    const char*         serializer_function { nullptr };
};

struct AttributeParser
{
    clang::SourceLocation       location;
    bool                        empty { false };
    bool                        is_field{ false };
    BEE_PAD(2);
    const char*                 current { nullptr };
    const char*                 end { nullptr };
    ReflectionAllocator*        allocator { nullptr };
    Diagnostics*                diagnostics { nullptr };

    bool init(const clang::Decl& decl, Diagnostics* new_diagnostics);

    void next();

    bool advance_on_char(char c);

    bool is_value_end() const;

    void skip_whitespace();

    bool parse(DynamicArray<Attribute>* dst_attributes, SerializationInfo* serialization_info, ReflectionAllocator* refl_allocator);

    bool parse_attribute(DynamicArray<Attribute>* dst_attributes, SerializationInfo* serialization_info);

    const char* parse_name();

    bool parse_value(Attribute* attribute);

    bool parse_string(Attribute* attribute);

    bool parse_symbol(Attribute* attribute);

    bool parse_number(Attribute* attribute);
};


/*
 *************************
 *
 * ASTMatcher
 *
 *************************
 */
struct ASTMatcher final : public clang::ast_matchers::MatchFinder::MatchCallback
{
    TypeMap*                            type_map {nullptr };
    ReflectionAllocator*                allocator { nullptr };
    Diagnostics                         diagnostics;
    llvm::SmallString<1024>             type_name;

    ASTMatcher(TypeMap* type_map_to_use, ReflectionAllocator* allocator_ptr);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    llvm::StringRef print_name(const clang::NamedDecl& decl);

    std::string print_qualtype_name(const clang::QualType& type, const clang::ASTContext& ast_context);

    void reflect_record(const clang::CXXRecordDecl& decl, RecordTypeStorage* parent = nullptr);

    void reflect_record_children(const clang::CXXRecordDecl& decl, RecordTypeStorage* storage);

    void reflect_enum(const clang::EnumDecl& decl, RecordTypeStorage* parent = nullptr);

    void reflect_field(const clang::FieldDecl& decl, const clang::ASTRecordLayout* enclosing_layout, RecordTypeStorage* parent);

    void reflect_function(const clang::FunctionDecl& decl, RecordTypeStorage* parent = nullptr);

    void reflect_array(const clang::FieldDecl& decl, RecordTypeStorage* parent, const clang::QualType& qualtype, AttributeParser* attr_parser);

    FieldStorage create_field(const llvm::StringRef& name, const i32 index, const clang::ASTContext& ast_context, const clang::ASTRecordLayout* enclosing_layout, const RecordTypeStorage* parent, const clang::QualType& qual_type, const clang::SourceLocation& location);
};


} // namespace reflect
} // namespace bee