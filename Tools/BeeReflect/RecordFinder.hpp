/*
 *  RecordFinder.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "ReflectionAllocator.hpp"

#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Path.hpp"

#include <clang/ASTMatchers/ASTMatchFinder.h>


namespace bee {


class ReflectionAllocator;


struct DynamicRecordType final : public RecordType
{
    DynamicArray<Field>          field_storage;
    DynamicArray<FunctionType>   function_storage;

    DynamicRecordType() = default;

    explicit DynamicRecordType(Allocator* allocator)
        : field_storage(allocator),
          function_storage(allocator)
    {}

    void add_field(const Field& field)
    {
        field_storage.push_back(field);
        fields = field_storage.span();
    }

    void add_function(const FunctionType* function)
    {
        function_storage.push_back(*function);
        functions = function_storage.span();
    }
};

struct DynamicFunctionType final : public FunctionType
{
    DynamicArray<Field> parameter_storage;

    DynamicFunctionType() = default;

    explicit DynamicFunctionType(Allocator* allocator)
        : parameter_storage(allocator)
    {}

    void add_parameter(const Field& field)
    {
        parameter_storage.push_back(field);
        parameters = parameter_storage.span();
    }
};


struct DynamicEnumType final : public EnumType
{
    DynamicArray<EnumConstant> constant_storage;

    DynamicEnumType() = default;

    explicit DynamicEnumType(Allocator* allocator)
        : constant_storage(allocator)
    {}

    void add_constant(const EnumConstant& constant)
    {
        constant_storage.push_back(constant);
        constants = constant_storage.span();
    }
};


struct TypeStorage
{
    DynamicArray<Type*>                              types;
    DynamicHashMap<Path, DynamicArray<const Type*>>  file_to_type_map;
    DynamicHashMap<u32, const Type*>                 hash_to_type_map;

    explicit TypeStorage(Allocator* allocator)
        : types(allocator),
          file_to_type_map(allocator),
          hash_to_type_map(allocator)
    {}

    Type* add_type(Type* type, const clang::Decl& decl);

    const Type* find_type(const u32 hash);
};


struct RecordFinder final : public clang::ast_matchers::MatchFinder::MatchCallback
{
    TypeStorage*                        storage { nullptr };
    DynamicRecordType*                  current_record { nullptr };
    ReflectionAllocator*                allocator { nullptr };
    llvm::SmallString<1024>             type_name;

    RecordFinder(TypeStorage* type_array, ReflectionAllocator* allocator_ptr);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& result) override;

    void reflect_record(const clang::CXXRecordDecl& decl);

    void reflect_enum(const clang::EnumDecl& decl);

    void reflect_field(const clang::FieldDecl& decl);

    void reflect_function(const clang::FunctionDecl& decl);

    Field create_field(const llvm::StringRef& name, const i32 index, const clang::ASTContext& ast_context, const clang::RecordDecl* parent, const clang::QualType& qual_type, const clang::SourceLocation& location, clang::DiagnosticsEngine& diagnostics);
};


} // namespace bee