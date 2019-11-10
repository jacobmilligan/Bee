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

    unsigned int    err_attribute_missing_equals { 0 };
    unsigned int    err_invalid_annotation_format { 0 };

    void init(clang::DiagnosticsEngine* diag_engine)
    {
        engine = diag_engine;

        err_attribute_missing_equals = engine->getCustomDiagID(clang::DiagnosticsEngine::Error, "invalid attribute format - missing '='");
        err_invalid_annotation_format = engine->getCustomDiagID(clang::DiagnosticsEngine::Error, "invalid reflection annotation - must be `bee-reflect`");
    }

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
struct AttributeParser
{
    const char*                 current { nullptr };
    const char*                 end { nullptr };
    DynamicArray<Attribute>*    dst { nullptr };
    ReflectionAllocator*        allocator { nullptr };
    Diagnostics*                diagnostics { nullptr };
    clang::SourceLocation       location;

    bool init(const clang::Decl& decl, Diagnostics* new_diagnostics);

    void next();

    void skip_whitespace();

    const char* parse_name();

    bool parse_value(Attribute* attribute);

    bool parse_attribute();

    bool parse(DynamicArray<Attribute>* dst_attributes, ReflectionAllocator* refl_allocator);
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
    DynamicRecordType*                  current_record { nullptr };
    ReflectionAllocator*                allocator { nullptr };
    Diagnostics                         diagnostics;
    llvm::SmallString<1024>             type_name;

    ASTMatcher(TypeStorage* type_storage, ReflectionAllocator* allocator_ptr);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    llvm::StringRef print_name(const clang::NamedDecl& decl);

    std::string print_qualtype_name(const clang::QualType& type, const clang::ASTContext& ast_context);

    void reflect_record(const clang::CXXRecordDecl& decl);

    void reflect_enum(const clang::EnumDecl& decl);

    void reflect_field(const clang::FieldDecl& decl);

    void reflect_function(const clang::FunctionDecl& decl);

    Field create_field(const llvm::StringRef& name, const i32 index, const clang::ASTContext& ast_context, const clang::RecordDecl* parent, const clang::QualType& qual_type, const clang::SourceLocation& location);
};


} // namespace reflect
} // namespace bee