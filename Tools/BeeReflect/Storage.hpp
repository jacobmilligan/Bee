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



class ReflectionAllocator
{
public:
    ReflectionAllocator(const size_t type_capacity, const size_t name_capacity);

    template <typename T, typename... Args>
    T* allocate_type(Args&&... args)
    {
        static_assert(std::is_base_of_v<Type, T>, "Cannot allocate a type that doesn't derive from bee::Type");
        return BEE_NEW(type_allocator_, T)(std::forward<Args>(args)...);
    }

    const char* allocate_name(const llvm::StringRef& src);
private:
    LinearAllocator         type_allocator_;
    LinearAllocator         name_allocator_;
};


struct TypeStorage
{
    DynamicArray<Type*>                                 types;
    DynamicHashMap<Path, DynamicArray<const Type*>>     file_to_type_map;
    DynamicHashMap<u32, const Type*>                    hash_to_type_map;
    DynamicArray<Path>                                  include_dirs;

    explicit TypeStorage(Allocator* allocator)
        : types(allocator),
          file_to_type_map(allocator),
          hash_to_type_map(allocator)
    {}

    bool try_map_type(Type* type);

    Type* add_type(Type* type, const clang::Decl& decl);

    const Type* find_type(const u32 hash);

    bool validate_and_reorder();
};


struct OrderedField final : public Field
{
    using Field::Field;

    i32                         order { -1 };
    clang::SourceLocation       location;
    DynamicArray<Attribute>     attributes;
};

inline bool operator==(const OrderedField& lhs, const OrderedField& rhs)
{
    return lhs.order == rhs.order;
}

inline bool operator!=(const OrderedField& lhs, const OrderedField& rhs)
{
    return lhs.order != rhs.order;
}

inline bool operator>(const OrderedField& lhs, const OrderedField& rhs)
{
    return lhs.order > rhs.order;
}

inline bool operator<(const OrderedField& lhs, const OrderedField& rhs)
{
    return lhs.order < rhs.order;
}

inline bool operator>=(const OrderedField& lhs, const OrderedField& rhs)
{
    return lhs.order >= rhs.order;
}

inline bool operator<=(const OrderedField& lhs, const OrderedField& rhs)
{
    return lhs.order <= rhs.order;
}


struct DynamicRecordType final : public RecordType
{
    const clang::CXXRecordDecl*             decl { nullptr };
    bool                                    has_explicit_version { false };
    DynamicArray<OrderedField>              field_storage;
    DynamicArray<FunctionType>              function_storage;
    DynamicArray<Attribute>                 attribute_storage;
    DynamicArray<const EnumType*>           enum_storage;
    DynamicArray<const RecordType*>         record_storage;
    const char*                             serializer_function_name { nullptr };

    DynamicRecordType() = default;

    explicit DynamicRecordType(const clang::CXXRecordDecl* parsed_decl, Allocator* allocator = system_allocator())
        : decl(parsed_decl),
          field_storage(allocator),
          function_storage(allocator),
          attribute_storage(allocator),
          enum_storage(allocator),
          record_storage(allocator)
    {}

    void add_field(const OrderedField& field)
    {
        field_storage.push_back(field);
        field_storage.back().attributes = field.attributes;
        fields = Span<Field>(field_storage.data(), field_storage.size());
    }

    void add_function(const FunctionType* function)
    {
        function_storage.push_back(*function);
        functions = function_storage.span();
    }

    void add_record(const RecordType* record)
    {
        record_storage.push_back(record);
        records = record_storage.span();
    }

    void add_enum(const EnumType* enum_type)
    {
        enum_storage.push_back(enum_type);
        enums = enum_storage.span();
    }
};

struct DynamicFunctionType final : public FunctionType
{
    DynamicArray<Field>         parameter_storage;
    DynamicArray<Attribute>     attribute_storage;
    DynamicArray<std::string>   invoker_type_args;

    DynamicFunctionType() = default;

    explicit DynamicFunctionType(Allocator* allocator)
        : parameter_storage(allocator),
          attribute_storage(allocator),
          invoker_type_args(allocator)
    {}

    void add_parameter(const Field& field)
    {
        parameter_storage.push_back(field);
        parameters = parameter_storage.span();
    }

    void add_attribute(const Attribute& attribute)
    {
        attribute_storage.push_back(attribute);
        attributes = attribute_storage.span();
    }

    void add_invoker_type_arg(const std::string& fully_qualified_name)
    {
        invoker_type_args.push_back(fully_qualified_name);
    }
};


struct DynamicEnumType final : public EnumType
{
    DynamicArray<EnumConstant>  constant_storage;
    DynamicArray<Attribute>     attribute_storage;

    DynamicEnumType() = default;

    explicit DynamicEnumType(Allocator* allocator)
        : constant_storage(allocator),
          attribute_storage(allocator)
    {}

    void add_constant(const EnumConstant& constant)
    {
        constant_storage.push_back(constant);
        constants = constant_storage.span();
    }

    void add_attribute(const Attribute& attribute)
    {
        attribute_storage.push_back(attribute);
        attributes = attribute_storage.span();
    }
};


} // namespace reflect
} // namespace bee