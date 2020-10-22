/*
 *  CodeGen.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "CodeGen.hpp"
#include "Storage.hpp"

#include "Bee/Core/IO.hpp"
#include "Bee/Core/Filesystem.hpp"

#include <inttypes.h>


namespace bee {
namespace reflect {


struct ModuleDataPtr
{
    uintptr_t local_offset { 0 };
    uintptr_t global_offset { 0 };
};

struct ModuleData
{
    void*               data { nullptr };
    DynamicArray<u8>    buffer;
};

struct ModuleWriter
{
    DynamicArray<ModuleData> data;
};


ReflString add_string(ModuleData* data, const StringView& string)
{
    ReflString result{};
    result.offset = data->buffer.size();
    data->buffer.append(0, string.size() + 1); // +1 null-terminator
    str::copy(reinterpret_cast<char*>(data->buffer.data() + result.offset), string.size(), string);
    return result;
}

template <typename T>
ReflPtr<T> add_ptr(ModuleData* data, T* dst)
{
    ReflPtr<T> result{};
    result.offset = data->buffer.size();
    data->buffer.append(0, sizeof(T));
    *dst = reinterpret_cast<T*>(data->buffer.data() + result.offset);
    new (dst) T{};
    return result;
}

template <typename T>
ReflArray<T> add_array(ModuleData* data, const i32 size, T** dst)
{
    ReflArray<T> result{};
    result.size = size;
    result.data.offset = data->buffer.size();
    data->buffer.resize(data->buffer.size() + sizeof(T) * size);
    *dst = reinterpret_cast<T*>(data->buffer.data() + result.data.offset);
    for (int i = 0; i < size; ++i)
    {
        new (&dst[i]) T{};
    }
    return result;
}

template <typename T>
ModuleData& add_data(ModuleWriter* writer)
{
    writer->data.emplace_back();
    auto& data = writer->data.back();
    data.buffer.resize(sizeof(T));
    new (reinterpret_cast<T*>(data.buffer.data())) T{};
    return data;
}

void dump_record(ModuleData* module_data, RecordTypeInfo* dst, const RecordTypeStorage* src)
{
    Field* fields = nullptr;
    dst->fields = add_array(module_data, src->fields.size(), &fields);

    for (const auto f : enumerate(src->fields))
    {
        const auto& field = f.value.field;
        fields[f.index].name = add_string(module_data, field.)
    }
}

void dump_reflection_module(const StringView& name, const Path& path, const ReflectedFile* files, const i32 count)
{
    ModuleWriter writer{};
    auto& module_data = add_data<ReflectionModule>(&writer);
    auto* module = reinterpret_cast<ReflectionModule*>(module_data.buffer.data());

    module->magic = reflection_module_magic;
    module->name = add_string(&module_data, name);

    int record_count = 0;
    int function_count = 0;
    int enum_count = 0;
    int array_count = 0;

    // get the total counts of all type kinds
    for (int i = 0; i < count; ++i)
    {
        record_count += files[i].records.size();
        function_count += files[i].functions.size();
        enum_count += files[i].enums.size();
        array_count += files[i].arrays.size();
    }

    ReflPtr<TypeInfo>* all_types = nullptr;
    module->all_types = add_array(
        &module_data,
        record_count + function_count + enum_count + array_count,
        &all_types
    );

    RecordTypeInfo* records = nullptr;
    FunctionTypeInfo* functions = nullptr;
    EnumTypeInfo* enums = nullptr;
    ArrayTypeInfo* arrays = nullptr;

    module->records = add_array(&module_data, record_count, &records);
    module->functions = add_array(&module_data, record_count, &functions);
    module->enums = add_array(&module_data, record_count, &enums);
    module->arrays = add_array(&module_data, record_count, &arrays);

    uintptr_t data_offset = module_data.buffer.size();

    for (int i = 0; i < count; ++i)
    {
        for (const auto r : enumerate(files[i].records))
        {
            ::bee::copy(&records[r.index], &r.value->type.data, 1);
            records[r.index].fields.data.offset += data_offset;

        }
    }
}


} // namespace reflect
} // namespace bee