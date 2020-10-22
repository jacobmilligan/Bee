/*
 *  ReflectionAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"

#include <llvm/ADT/StringRef.h>
#include <clang/AST/DeclBase.h>

namespace bee {


class LinearAllocator;


namespace reflect {

/*
 ***************************************
 *
 * Global allocator for bee-reflect.
 * Allocates memory in 4mb chunks
 *
 ***************************************
 */
extern Allocator*       g_allocator;

struct TypeMap;

struct ReflectedFile
{
    u32                                         hash { 0 };
    Path                                        location { g_allocator };
    DynamicArray<const TypeInfo*>               types { g_allocator };
};

struct AttributeStorage
{
    String      name;
    String      string_value;
    Attribute   data;

    explicit AttributeStorage(Allocator* allocator)
        : name(allocator),
          string_value(allocator)
    {}
};

struct PtrFixup
{
    uintptr_t offset_in_parent { 0 };
    uintptr_t offset_in_buffer { 0 };
};

struct TypeFixup
{
    i32         target_type_index { 0 };
    uintptr_t   offset_in_parent { 0 };
};

struct TypeBuffer
{
    i32                     index { 0 };
    uintptr_t               global_offset { 0 };
    TypeInfo*               type { nullptr };
    DynamicArray<u8>        buffer;
    DynamicArray<PtrFixup>  fixups;
    DynamicArray<TypeFixup> type_fixups;

    TypeBuffer(TypeInfo* new_type, const i32 initial_buffer_size)
        : type(new_type),
          buffer(initial_buffer_size, 0, g_allocator),
          fixups(g_allocator)
    {}
};

void* type_buffer_alloc(TypeBuffer* buffer, const size_t size, const uintptr_t offset_in_parent);

template <typename T>
TypeBuffer* make_type_buffer(TypeMap* map)
{
    auto* mem = BEE_MALLOC(g_allocator, sizeof(TypeBuffer) + sizeof(T));
    auto* buffer = static_cast<TypeBuffer*>(mem);
    new (buffer) TypeBuffer(reinterpret_cast<TypeInfo*>(static_cast<u8*>(mem) + sizeof(T)), sizeof(T));
    new (reinterpret_cast<T*>(buffer->type)) T{};
    buffer->index = map->buffers.size();
    map->buffers.emplace_back();
    return buffer;
}


template <typename T>
struct TypeBufferWriter
{
    T*          type { nullptr };
    TypeBuffer* buffer { nullptr };

    explicit TypeBufferWriter(TypeBuffer* dst_buffer)
        : buffer(dst_buffer),
          type(reinterpret_cast<T*>(dst_buffer->type))
    {}

    inline uintptr_t get_offset()
    {
        return buffer->buffer.size();
    }

    template <typename ValueType>
    void write(ValueType T::*member, const ValueType& value)
    {
        ::bee::copy(&type->*member, &value, sizeof(ValueType));
    }

    template <typename ValueType>
    void write(ValueType TypeInfo::*member, const ValueType& value)
    {
        ::bee::copy(&type->*member, &value, sizeof(ValueType));
    }

    template <typename ValueType>
    ValueType* write_array(ReflArray<ValueType> T::*member, const i32 size)
    {
        const uintptr_t offset_in_parent = reinterpret_cast<u8*>(&type->*member.data) - reinterpret_cast<u8*>(type);
        auto* alloc = type_buffer_alloc(buffer, sizeof(ValueType) * size, offset_in_parent);
        &type->*member.size = size;
        return static_cast<ValueType*>(alloc);
    }

    template <typename ValueType>
    ValueType* write_array(ReflArray<ValueType> TypeInfo::*member, const i32 size)
    {
        const uintptr_t offset_in_parent = reinterpret_cast<u8*>(&type->*member.data) - reinterpret_cast<u8*>(type);
        auto* alloc = type_buffer_alloc(buffer, sizeof(ValueType) * size, offset_in_parent);
        &type->*member.size = size;
        return static_cast<ValueType*>(alloc);
    }

    template <typename ParentType, typename ValueType>
    ValueType* write_external_array(ParentType* parent, ReflArray<ValueType> ParentType::*member, const i32 size)
    {
        auto* as_bytes = reinterpret_cast<u8*>(parent);
        BEE_ASSERT(as_bytes >= buffer->buffer.data() && as_bytes < buffer->buffer.data());

        const uintptr_t external_offset = as_bytes - buffer->buffer.data();
        const uintptr_t offset_in_parent = reinterpret_cast<u8*>(&parent->*member.data) - as_bytes;

        auto* alloc = type_buffer_alloc(buffer, sizeof(ValueType) * size, external_offset + offset_in_parent);
        &parent->*member.size = size;
        return static_cast<ValueType*>(alloc);
    }

    void write_string(ReflString T::*member, const StringView& src)
    {
        const uintptr_t offset_in_parent = reinterpret_cast<u8*>(&type->*member.data) - reinterpret_cast<u8*>(type);
        const size_t dst_size = sizeof(char) * src.size() + 1; // for null-terminator
        auto* alloc = type_buffer_alloc(buffer, dst_size, offset_in_parent);
        str::copy(static_cast<char*>(alloc), dst_size, src);
    }

    template <typename ParentType>
    void write_external_string(ParentType* parent, ReflString ParentType::*member, const StringView& src)
    {
        auto* as_bytes = reinterpret_cast<u8*>(parent);
        BEE_ASSERT(as_bytes >= buffer->buffer.data() && as_bytes < buffer->buffer.data());

        const uintptr_t external_offset = as_bytes - buffer->buffer.data();
        const uintptr_t offset_in_parent = reinterpret_cast<u8*>(&parent->*member.data) - as_bytes;
        const size_t dst_size = sizeof(char) * src.size() + 1; // for null-terminator

        // Alloc with the offset outside the end of the type so we can still do a proper fixup
        auto* alloc = type_buffer_alloc(buffer, dst_size, sizeof(T) + external_offset + offset_in_parent);
        str::copy(static_cast<char*>(alloc), dst_size, src);
    }

    void write_attributes(ReflArray<Attribute> T::*member, const Span<const AttributeStorage>& attributes)
    {
        if (attributes.empty())
        {
            type->*member.data.offset = 0;
            type->*member.size = 0;
            return;
        }

        auto* array = write_array(member, attributes.size());
        for (auto attr : enumerate(attributes))
        {
            ::bee::copy(&array[attr.index], &attr.value.data, 1);
            if (!attr.value.name.empty())
            {
                write_external_string(&array[attr.index], &Attribute::name, attr.value.name.view());
            }
            if (!attr.value.string_value.empty())
            {
                write_external_string(&array[attr.index], &Attribute::value, attr.value.name.view());
            }
        }
    }

    template <typename ParentType>
    void write_external_attributes(ParentType* parent, ReflArray<Attribute> ParentType::*member, const Span<const AttributeStorage>& attributes)
    {
        if (attributes.empty())
        {
            type->*member.data.offset = 0;
            type->*member.size = 0;
            return;
        }

        auto* array = write_external_array(parent, member, attributes.size());
        for (auto attr : enumerate(attributes))
        {
            ::bee::copy(&array[attr.index], &attr.value.data, 1);
            if (!attr.value.name.empty())
            {
                write_external_string(&array[attr.index], &Attribute::name, attr.value.name.view());
            }
        }
    }

};

struct TypeMap
{
    DynamicHashMap<u32, ReflectedFile>      reflected_files { g_allocator };
    DynamicHashMap<u32, const TypeInfo*>    type_lookup { g_allocator };
    DynamicArray<Path>                      include_dirs { g_allocator };
    DynamicArray<const TypeInfo*>           all_types { g_allocator };
    DynamicArray<TypeBuffer*>               buffers;
};

struct TempAllocScope
{
    size_t      offset { 0 };
    Allocator*  allocator { nullptr };

    TempAllocScope();

    ~TempAllocScope();

    inline operator Allocator*() // NOLINT
    {
        return allocator;
    }
};

void type_map_add(TypeMap* map, const TypeInfo* type, const clang::Decl& decl);

const TypeInfo* type_map_find(TypeMap* map, const u32 hash);

template <typename T>
void copy_refl_ptr(ReflPtr<T>* from, ReflPtr<T>* to)
{
    *from = *to;
    to->offset += reinterpret_cast<u8*>(from) - reinterpret_cast<u8*>(to);
}



} // namespace reflect
} // namespace bee