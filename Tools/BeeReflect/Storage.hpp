/*
 *  ReflectionAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Memory/SmartPointers.hpp"

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
    DynamicArray<Type>          template_arguments;
    const char*                 specialized_type {nullptr };
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


struct ArrayTypeStorage;
struct FunctionTypeStorage;
struct EnumTypeStorage;
struct ReflectedFile;


struct RecordTypeStorage
{
    ReflectedFile*                          location { nullptr };
    const clang::CXXRecordDecl*             decl { nullptr };
    bool                                    has_explicit_version { false };
    RecordTypeInfo                              type;
    DynamicArray<FieldStorage>              fields;
    DynamicArray<Attribute>                 attributes;
    DynamicArray<FunctionTypeStorage*>      functions;
    DynamicArray<EnumTypeStorage*>          enums;
    DynamicArray<RecordTypeStorage*>        nested_records;
    DynamicArray<const char*>               base_type_names;
    DynamicArray<ArrayTypeStorage*>         field_array_types;
    DynamicArray<TemplateParameter>         template_parameters;
    const char*                             template_decl_string { nullptr };

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

    void add_array_type(ArrayTypeStorage* array_type);

    void add_template_parameter(const TemplateParameter& param);
};

struct FunctionTypeStorage
{
    ReflectedFile*              location { nullptr };
    FunctionTypeInfo            type;
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
    EnumTypeInfo                    type;
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

struct ArrayTypeStorage
{
    const char* element_type_name { nullptr };
    bool        is_generated { false };
    bool        uses_builder {false };
    ArrayTypeInfo   type;
};


class ReflectionAllocator
{
public:
    ReflectionAllocator(const size_t type_capacity, const size_t name_capacity);

    template <typename T, typename... Args>
    T* allocate_storage(Args&&... args)
    {
        auto ptr = BEE_NEW(type_allocator_, T)(std::forward<Args>(args)...);

        allocations_.emplace_back();

        auto& alloc = allocations_.back();
        alloc.allocator = &type_allocator_;
        alloc.data = ptr;
        alloc.destructor = [](Allocator* allocator, void* data)
        {
            BEE_DELETE(allocator, static_cast<T*>(data));
        };

        return ptr;
    }

    const char* allocate_name(const llvm::StringRef& src);
private:
    struct Allocation
    {
        Allocator* allocator { nullptr };
        void (*destructor)(Allocator*, void*) { nullptr };
        void* data { nullptr };

        ~Allocation()
        {
            destructor(allocator, data);
        }
    };

    LinearAllocator             type_allocator_;
    LinearAllocator             name_allocator_;
    DynamicArray<Allocation>    allocations_;
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
    DynamicArray<ArrayTypeStorage*>             arrays;
    DynamicArray<const TypeInfo*>               all_types;

    bool try_insert_type(const TypeInfo* type) const;
};


struct TypeMap
{
    struct MappedType
    {
        u32             owning_file_hash { 0 };
        const TypeInfo* type { nullptr };
    };
    DynamicHashMap<u32, ReflectedFile>      reflected_files;
    DynamicHashMap<u32, MappedType>         type_lookup;
    DynamicArray<Path>                      include_dirs;

    explicit TypeMap(Allocator* allocator)
        : reflected_files(allocator),
          type_lookup(allocator),
          include_dirs(allocator)
    {}

    bool try_add_type(const TypeInfo* type, const clang::Decl& decl, ReflectedFile** reflected_file);

    void add_array(ArrayTypeStorage* type, const clang::Decl& decl);

    void add_record(RecordTypeStorage* record, const clang::Decl& decl);

    void add_function(FunctionTypeStorage* function, const clang::Decl& decl);

    void add_enum(EnumTypeStorage* enum_storage, const clang::Decl& decl);

    const TypeInfo* find_type(const u32 hash);
};


} // namespace reflect
} // namespace bee