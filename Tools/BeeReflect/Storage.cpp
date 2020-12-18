/*
 *  Storage.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Storage.hpp"

#include <clang/AST/ASTContext.h>

namespace bee {
namespace reflect {


ReflectionAllocator::ReflectionAllocator(
    const size_t type_capacity,
    const size_t name_capacity
) : type_allocator_(type_capacity),
    name_allocator_(name_capacity)
{}

const char* ReflectionAllocator::allocate_name(const llvm::StringRef& src)
{
    static constexpr const char* empty_string = "";

    auto* data = static_cast<char*>(BEE_MALLOC(name_allocator_, src.size() + 1));
    if (data == nullptr)
    {
        return empty_string;
    }

    allocations_.emplace_back();
    allocations_.back().allocator = &name_allocator_;
    allocations_.back().data = data;
    allocations_.back().destructor = [](Allocator* allocator, void* data)
    {
        BEE_FREE(allocator, data);
    };

    memcpy(data, src.data(), src.size());
    data[src.size()] = '\0';
    return data;
}


bool ReflectedFile::try_insert_type(const TypeInfo* type) const
{
    BEE_ASSERT(parent_map != nullptr);

    if (parent_map->find_type(type->hash) != nullptr)
    {
        return false;
    }

    TypeMap::MappedType mapped_type{};
    mapped_type.owning_file_hash = hash;
    mapped_type.type = type;
    return parent_map->type_lookup.insert(type->hash, mapped_type) != nullptr;
}


bool TypeMap::try_add_type(const TypeInfo* type, const clang::Decl& decl, ReflectedFile** reflected_file)
{
    BEE_ASSERT(reflected_file != nullptr);

    auto* existing_type = type_lookup.find(type->hash);
    if (existing_type != nullptr)
    {
        *reflected_file = &reflected_files.find(existing_type->value.owning_file_hash)->value;
        return false;
    }

    auto& src_manager = decl.getASTContext().getSourceManager();


    llvm::StringRef filename_ref{};

    if (decl.getLocation().isFileID())
    {
        const auto* file = src_manager.getFileEntryForID(src_manager.getFileID(decl.getLocation()));
        filename_ref = file->getName();
    }
    else
    {
        const auto file_loc = src_manager.getFileLoc(src_manager.getExpansionLoc(decl.getLocation()));
        const auto* file = src_manager.getFileEntryForID(src_manager.getFileID(file_loc));
        filename_ref = file->getName();
    }

    String filepath(StringView(filename_ref.data(), filename_ref.size()), temp_allocator());
    str::replace(&filepath, Path::preferred_slash, Path::generic_slash); // normalize the slashes

    for (const auto& include_dir : include_dirs)
    {
        // Make the filepath relative to the discovered include dir if available
        if (str::first_index_of(filepath.view(), include_dir.view()) == 0)
        {
            auto length = include_dir.size();
            if (filepath[include_dir.size()] == Path::preferred_slash || filepath[include_dir.size()] == Path::generic_slash)
            {
                ++length; // remove leading slash
            }

            filepath.remove(0, length);
            break;
        }
    }

    MappedType mapped_type{};
    mapped_type.type = type;
    mapped_type.owning_file_hash = get_hash(filepath);
    type_lookup.insert(type->hash, mapped_type);

    auto* file_keyval = reflected_files.find(mapped_type.owning_file_hash);

    if (file_keyval == nullptr)
    {
        file_keyval = reflected_files.insert(
            mapped_type.owning_file_hash,
            ReflectedFile(mapped_type.owning_file_hash, filepath.view(), this)
        );
    }

    *reflected_file = &file_keyval->value;
    return true;
}

void TypeMap::add_array(ArrayTypeStorage* array, const clang::Decl& decl)
{
    ReflectedFile* location = nullptr;
    if (!try_add_type(&array->type, decl, &location))
    {
        return;
    }
    location->arrays.push_back(array);
    location->all_types.emplace_back(array);
}

void TypeMap::add_record(RecordTypeStorage* record, const clang::Decl& decl)
{
    if (!try_add_type(&record->type, decl, &record->location))
    {
        return;
    }
    record->location->records.push_back(record);
    record->location->all_types.emplace_back(record);
}

void TypeMap::add_function(FunctionTypeStorage* function, const clang::Decl& decl)
{
    if (!try_add_type(&function->type, decl, &function->location))
    {
        return;
    }
    function->location->functions.push_back(function);
    function->location->all_types.emplace_back(function);
}

void TypeMap::add_enum(EnumTypeStorage* enum_storage, const clang::Decl& decl)
{
    if (!try_add_type(&enum_storage->type, decl, &enum_storage->location))
    {
        return;
    }
    enum_storage->location->enums.push_back(enum_storage);
    enum_storage->location->all_types.emplace_back(enum_storage);
}

const TypeInfo* TypeMap::find_type(const u32 hash)
{
    auto* type = type_lookup.find(hash);
    return type == nullptr ? nullptr : type->value.type;
}


void RecordTypeStorage::add_field(const FieldStorage& field)
{
    fields.push_back(field);
}

void RecordTypeStorage::add_function(FunctionTypeStorage* storage)
{
    if (!location->try_insert_type(&storage->type))
    {
        return;
    }

    storage->location = location;
    functions.push_back(storage);
}

void RecordTypeStorage::add_record(RecordTypeStorage* storage)
{
    if (!location->try_insert_type(&storage->type))
    {
        return;
    }

    storage->location = location;
    nested_records.push_back(storage);
}

void RecordTypeStorage::add_enum(EnumTypeStorage* storage)
{
    if (!location->try_insert_type(&storage->type))
    {
        return;
    }

    storage->location = location;
    enums.push_back(storage);
}

void RecordTypeStorage::add_array_type(ArrayTypeStorage* storage)
{
    if (!location->try_insert_type(&storage->type))
    {
        return;
    }

    field_array_types.push_back(storage);
}

void RecordTypeStorage::add_template_parameter(const TemplateParameter& param)
{
    template_parameters.push_back(param);
}

void FunctionTypeStorage::add_parameter(const FieldStorage& field)
{
    parameters.push_back(field);
}

void FunctionTypeStorage::add_attribute(const Attribute& attribute)
{
    attributes.push_back(attribute);
}

void FunctionTypeStorage::add_invoker_type_arg(const std::string& fully_qualified_name)
{
    invoker_type_args.push_back(fully_qualified_name);
}

void EnumTypeStorage::add_constant(const EnumConstant& constant)
{
    constants.push_back(constant);
}

void EnumTypeStorage::add_attribute(const Attribute& attribute)
{
    attributes.push_back(attribute);
}


} // namespace reflect
} // namespace bee