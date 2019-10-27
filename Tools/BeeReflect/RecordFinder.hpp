/*
 *  RecordFinder.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "ReflectionAllocator.hpp"

#include "Bee/Core/Containers/HashMap.hpp"

#include <clang/ASTMatchers/ASTMatchFinder.h>


namespace bee {


class ReflectionAllocator;


struct RecordFinder final : public clang::ast_matchers::MatchFinder::MatchCallback
{
    RecordType*                         current_record { nullptr };
    DynamicArray<Type*>*                types {nullptr };
    DynamicHashMap<u32, const Type*>    type_lookup;
    ReflectionAllocator*                allocator { nullptr };
    llvm::SmallString<1024>             type_name;

    RecordFinder(DynamicArray<Type*>* type_array, ReflectionAllocator* allocator_ptr);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    void reflect_record(const clang::CXXRecordDecl& decl);

    void reflect_field(const clang::FieldDecl& decl);

    void reflect_function(const clang::FunctionDecl& decl);

    Field create_field(const llvm::StringRef& name, const i32 index, const clang::ASTContext& ast_context, const clang::RecordDecl* parent, const clang::QualType& qual_type, const clang::SourceLocation& location, clang::DiagnosticsEngine& diagnostics);

    void add_type(Type* type);
};


} // namespace bee