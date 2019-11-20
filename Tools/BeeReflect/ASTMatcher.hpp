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

    void init(clang::DiagnosticsEngine* diag_engine);

    inline clang::DiagnosticBuilder Report(clang::SourceLocation location, unsigned diag_id)
    {
        return engine->Report(location, diag_id);
    }
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
    static constexpr auto serialized_attr_name = "serializable";
    static constexpr auto nonserialized_attr_name = "nonserialized";
    static constexpr auto version_attr_name = "version";
    static constexpr auto version_added_attr_name = "added";
    static constexpr auto version_removed_attr_name = "removed";
    static constexpr auto id_attr_name = "id";

    static const u32 serialized_hash;
    static const u32 nonserialized_hash;
    static const u32 version_hash;
    static const u32 version_added_hash;
    static const u32 version_removed_hash;
    static const u32 id_hash;

    bool    serializable { false };
    bool    using_explicit_versioning { false };
    i32     serialized_version { 0 };
    i32     version_added { 0 };
    i32     version_removed { limits::max<i32>() };
    i32     id { -1 };
};

struct AttributeParser
{
    bool                        empty { false };
    bool                        is_field{ false };
    const char*                 current { nullptr };
    const char*                 end { nullptr };
    ReflectionAllocator*        allocator { nullptr };
    Diagnostics*                diagnostics { nullptr };
    clang::SourceLocation       location;

    bool init(const clang::Decl& decl, Diagnostics* new_diagnostics);

    void next();

    void skip_whitespace();

    const char* parse_name();

    bool parse_value(Attribute* attribute);

    bool parse_attribute(DynamicArray<Attribute>* dst_attributes, SerializationInfo* serialization_info);

    bool parse(DynamicArray<Attribute>* dst_attributes, SerializationInfo* serialization_info, ReflectionAllocator* refl_allocator);
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
    TypeStorage*                        storage { nullptr };
    ReflectionAllocator*                allocator { nullptr };
    Diagnostics                         diagnostics;
    llvm::SmallString<1024>             type_name;

    ASTMatcher(TypeStorage* type_storage, ReflectionAllocator* allocator_ptr);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    llvm::StringRef print_name(const clang::NamedDecl& decl);

    std::string print_qualtype_name(const clang::QualType& type, const clang::ASTContext& ast_context);

    void reflect_record(const clang::CXXRecordDecl& decl, DynamicRecordType* parent = nullptr);

    void reflect_enum(const clang::EnumDecl& decl, DynamicRecordType* parent = nullptr);

    void reflect_field(const clang::FieldDecl& decl, DynamicRecordType* parent);

    void reflect_function(const clang::FunctionDecl& decl, DynamicRecordType* parent = nullptr);

    template <typename FieldType>
    FieldType create_field(const llvm::StringRef& name, const i32 index, const clang::ASTContext& ast_context, const DynamicRecordType* parent, const clang::QualType& qual_type, const clang::SourceLocation& location)
    {
        static_assert(std::is_base_of_v<Field, FieldType>, "FieldType must derive from bee::Field");
        /*
        * Get the layout of the parent record this field is in and also get pointers to the desugared type so that
        * i.e. u8 becomes unsigned char
        */
        auto desugared_type = qual_type.getDesugaredType(ast_context);

        FieldType field{};
        field.name = allocator->allocate_name(name);
        field.offset = 0;
        field.qualifier = get_qualifier(desugared_type);

        if (parent == nullptr)
        {
            diagnostics.Report(location, clang::diag::err_incomplete_type);
            return FieldType{};
        }

        if (parent != nullptr && index >= 0)
        {
            field.offset = ast_context.getASTRecordLayout(parent->decl).getFieldOffset(static_cast<u32>(index)) / 8;
        }

        clang::QualType original_type;
        auto type_ptr = desugared_type.getTypePtrOrNull();

        // Check if reference or pointer and get the pointee and const-qualified info before removing qualifications
        if (type_ptr != nullptr && (type_ptr->isPointerType() || type_ptr->isLValueReferenceType()))
        {
            const auto pointee = type_ptr->getPointeeType();

            if (pointee.isConstQualified())
            {
                field.qualifier |= Qualifier::cv_const;
            }

            original_type = pointee.getUnqualifiedType();
        }
        else
        {
            original_type = desugared_type.getUnqualifiedType();
        }

        // Get the associated types hash so we can look it up later
        const auto fully_qualified_name = print_qualtype_name(original_type, ast_context);
        const auto type_hash = get_type_hash({ fully_qualified_name.data(), static_cast<i32>(fully_qualified_name.size()) });

        auto type = storage->find_type(type_hash);
        if (type == nullptr)
        {
            /*
             * If the type is missing it might be a core builtin type which can be accessed by bee-reflect via
             * get_type. This is safe to do here as bee-reflect doesn't link to any generated cpp files
             */
            type = get_type(type_hash);
            if (type->kind == TypeKind::unknown)
            {
                diagnostics.Report(location, diagnostics.warn_unknown_field_type).AddString(fully_qualified_name);
            }
        }

        field.hash = get_type_hash(field.name);
        field.type = type;
        return field;
    }
};


} // namespace reflect
} // namespace bee