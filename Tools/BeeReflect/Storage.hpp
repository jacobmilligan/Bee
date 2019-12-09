/*
 *  ReflectionAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/ReflectionV2.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

#include <llvm/ADT/StringRef.h>
#include <clang/AST/DeclBase.h>

namespace bee {
namespace reflect {


struct FieldStorage
{
    Field                       field;
    i32                         order { -1 };
    clang::SourceLocation       location;
    DynamicArray<Attribute>     attributes;
    DynamicArray<const Type*>   template_arguments;
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


struct FunctionTypeStorage;
struct EnumTypeStorage;
struct ReflectedFile;


struct RecordTypeStorage
{
    ReflectedFile*                      location { nullptr };
    const clang::CXXRecordDecl*         decl { nullptr };
    bool                                has_explicit_version { false };
    RecordType                          type;
    DynamicArray<FieldStorage>          fields;
    DynamicArray<Attribute>             attributes;
    DynamicArray<FunctionTypeStorage*>  functions;
    DynamicArray<EnumTypeStorage*>      enums;
    DynamicArray<RecordTypeStorage*>    nested_records;
    DynamicArray<ArrayType*>            field_array_types;
    DynamicArray<TemplateParameter>     template_parameters;
    const char*                         serializer_function_name { nullptr };

    RecordTypeStorage() = default;

    explicit RecordTypeStorage(const clang::CXXRecordDecl* parsed_decl, Allocator* allocator = system_allocator())
        : decl(parsed_decl),
          fields(allocator),
          functions(allocator),
          attributes(allocator),
          enums(allocator),
          nested_records(allocator),
          field_array_types(allocator)
    {}

    void add_field(const FieldStorage& field);

    void add_function(FunctionTypeStorage* storage);

    void add_record(RecordTypeStorage* storage);

    void add_enum(EnumTypeStorage* storage);

    void add_array_type(ArrayType* array_type);

    void add_template_parameter(const TemplateParameter& param);
};

struct FunctionTypeStorage
{
    ReflectedFile*              location { nullptr };
    FunctionType                type;
    FieldStorage                return_field;
    DynamicArray<FieldStorage>  parameters;
    DynamicArray<Attribute>     attributes;
    DynamicArray<std::string>   invoker_type_args;

    FunctionTypeStorage() = default;

    explicit FunctionTypeStorage(Allocator* allocator)
        : parameters(allocator),
          attributes(allocator),
          invoker_type_args(allocator)
    {}

    void add_parameter(const FieldStorage& field);

    void add_attribute(const Attribute& attribute);

    void add_invoker_type_arg(const std::string& fully_qualified_name);
};


struct EnumTypeStorage
{
    ReflectedFile*              location { nullptr };
    EnumType                    type;
    DynamicArray<EnumConstant>  constants;
    DynamicArray<Attribute>     attributes;

    EnumTypeStorage() = default;

    explicit EnumTypeStorage(Allocator* allocator)
        : constants(allocator),
          attributes(allocator)
    {}

    void add_constant(const EnumConstant& constant);

    void add_attribute(const Attribute& attribute);
};


class ReflectionAllocator
{
public:
    ReflectionAllocator(const size_t type_capacity, const size_t name_capacity);

    template <typename T, typename... Args>
    T* allocate_storage(Args&&... args)
    {
        return BEE_NEW(type_allocator_, T)(std::forward<Args>(args)...);
    }

    const char* allocate_name(const llvm::StringRef& src);
private:
    LinearAllocator         type_allocator_;
    LinearAllocator         name_allocator_;
};


struct TypeMap;


struct ReflectedFile
{
    ReflectedFile() = default;

    explicit ReflectedFile(const u32 new_hash, const StringView& location_str, TypeMap* new_parent_map, Allocator* allocator = system_allocator())
        : hash(new_hash),
          location(location_str, allocator),
          parent_map(new_parent_map),
          records(allocator),
          functions(allocator),
          enums(allocator),
          arrays(allocator),
          all_types(allocator)
    {}

    u32                                         hash { 0 };
    TypeMap*                                    parent_map { nullptr };
    Path                                        location;
    DynamicArray<const RecordTypeStorage*>      records;
    DynamicArray<const FunctionTypeStorage*>    functions;
    DynamicArray<const EnumTypeStorage*>        enums;
    DynamicArray<const ArrayType*>              arrays;
    DynamicArray<const Type*>                   all_types;

    bool try_insert_type(const Type* type);
};


struct TypeMap
{
    struct MappedType
    {
        u32         owning_file_hash { 0 };
        const Type* type { nullptr };
    };
    DynamicHashMap<u32, ReflectedFile>      reflected_files;
    DynamicHashMap<u32, MappedType>         type_lookup;
    DynamicArray<Path>                      include_dirs;

    explicit TypeMap(Allocator* allocator)
        : reflected_files(allocator),
          type_lookup(allocator),
          include_dirs(allocator)
    {}

    bool try_add_type(const Type* type, const clang::Decl& decl, ReflectedFile** reflected_file);

    void add_array(ArrayType* type, const clang::Decl& decl);

    void add_record(RecordTypeStorage* record, const clang::Decl& decl);

    void add_function(FunctionTypeStorage* function, const clang::Decl& decl);

    void add_enum(EnumTypeStorage* enum_storage, const clang::Decl& decl);

    const Type* find_type(const u32 hash);
};


} // namespace reflect
} // namespace bee