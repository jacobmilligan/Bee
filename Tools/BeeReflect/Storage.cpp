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

    auto data = static_cast<char*>(BEE_MALLOC(name_allocator_, src.size() + 1));
    if (data == nullptr)
    {
        return empty_string;
    }

    memcpy(data, src.data(), src.size());
    data[src.size()] = '\0';
    return data;
}


bool TypeStorage::try_map_type(Type* type)
{
    if (hash_to_type_map.find(type->hash) != nullptr)
    {
        return false;
    }

    return hash_to_type_map.insert(type->hash, type) != nullptr;
}


Type* TypeStorage::add_type(Type* type, const clang::Decl& decl)
{
    auto& src_manager = decl.getASTContext().getSourceManager();
    const auto file = src_manager.getFileEntryForID(src_manager.getFileID(decl.getLocation()));
    const auto filename_ref = file->getName();
    String filepath(StringView(filename_ref.data(), filename_ref.size()), temp_allocator());

    if (hash_to_type_map.find(type->hash) != nullptr)
    {
        return nullptr;
    }

    for (const auto& include_dir : include_dirs)
    {
        // Make the filepath relative to the discovered include dir if available
        if (filename_ref.startswith(include_dir.c_str()))
        {
            auto length = include_dir.size();
            if (filepath[include_dir.size()] == Path::preferred_slash || filepath[include_dir.size()] == Path::generic_slash)
            {
                ++length; // remove leading slash
            }

            filepath.remove(0, length);
            // Make generic path format
            str::replace(&filepath, '\\', '/');
            break;
        }
    }

    types.emplace_back(type);

    hash_to_type_map.insert(type->hash, type);

    auto mapped_file = file_to_type_map.find(filepath.view());

    if (mapped_file == nullptr)
    {
        mapped_file = file_to_type_map.insert(Path(filepath.view()), DynamicArray<const Type*>());
    }

    mapped_file->value.push_back(type);

    return types.back();

}

const Type* TypeStorage::find_type(const u32 hash)
{
    auto type = hash_to_type_map.find(hash);
    return type == nullptr ? nullptr : type->value;
}

bool TypeStorage::validate_and_reorder()
{
    return true;
}


} // namespace reflect
} // namespace bee