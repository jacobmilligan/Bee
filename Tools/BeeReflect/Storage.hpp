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

    template <typename T>
    T* allocate_type()
    {
        static_assert(std::is_base_of_v<Type, T>, "Cannot allocate a type that doesn't derive from bee::Type");
        return BEE_NEW(type_allocator_, T);
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

    explicit TypeStorage(Allocator* allocator)
        : types(allocator),
          file_to_type_map(allocator),
          hash_to_type_map(allocator)
    {}

    Type* add_type(Type* type, const clang::Decl& decl);

    const Type* find_type(const u32 hash);
};


struct DynamicRecordType final : public RecordType
{
    DynamicArray<DynamicArray<Attribute>>   field_attributes;
    DynamicArray<Field>                     field_storage;
    DynamicArray<FunctionType>              function_storage;
    DynamicArray<Attribute>                 attribute_storage;

    DynamicRecordType() = default;

    explicit DynamicRecordType(Allocator* allocator)
        : field_storage(allocator),
          function_storage(allocator),
          attribute_storage(allocator)
    {}

    void add_field(const Field& field, DynamicArray<Attribute>&& attributes)
    {
        field_storage.push_back(field);
        field_attributes.emplace_back(std::move(attributes));
        fields = Span<Field>(field_storage.data(), field_storage.size());
        field_storage.back().attributes = field_attributes.back().span();
    }

    void add_function(const FunctionType* function)
    {
        function_storage.push_back(*function);
        functions = function_storage.span();
    }
};

struct DynamicFunctionType final : public FunctionType
{
    DynamicArray<Field>     parameter_storage;
    DynamicArray<Attribute> attribute_storage;

    DynamicFunctionType() = default;

    explicit DynamicFunctionType(Allocator* allocator)
        : parameter_storage(allocator),
          attribute_storage(allocator)
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