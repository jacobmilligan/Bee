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


Allocator* g_allocator = system_allocator();
LinearAllocator g_temp_allocator;


TempAllocScope::TempAllocScope()
    : offset(g_temp_allocator.offset()),
      allocator(&g_temp_allocator)
{}

TempAllocScope::~TempAllocScope()
{
    g_temp_allocator.reset_offset(offset);
}

void* type_buffer_alloc(TypeBuffer* buffer, const size_t size, const uintptr_t offset_in_parent)
{
    TypeBuffer::PtrFixup fixup{};
    fixup.offset_in_buffer = buffer->buffer.size();
    fixup.offset_in_parent = offset_in_parent;
    buffer->fixups.push_back(fixup);
    buffer->buffer.resize(buffer->buffer.size() + size);
    return buffer->buffer.data() + fixup.offset_in_buffer;
}

void add_type(TypeMap* map, const TypeInfo* info, const clang::Decl& decl)
{
    BEE_ASSERT(info != nullptr);

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

    for (const auto& include_dir : map->include_dirs)
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

    map->type_lookup.insert(info->hash, info);
    map->all_types.push_back(info);

    const u32 file_hash = get_hash(filepath);
    auto* file_keyval = map->reflected_files.find(file_hash);

    if (file_keyval == nullptr)
    {
        file_keyval = map->reflected_files.insert(file_hash, ReflectedFile{});
    }

    file_keyval->value.types.push_back(info);
}

const TypeInfo* type_map_find(TypeMap* map, const u32 hash)
{
    auto* type = map->type_lookup.find(hash);
    return type == nullptr ? nullptr : type->value;
}

void type_map_add(TypeMap* map, const TypeInfo* type, const clang::Decl& decl)
{
    if (type_map_find(map, type->hash) != nullptr)
    {
        log_warning("bee-reflect: Type %s is already mapped", type->name.get());
        return;
    }
    add_type(map, type, decl);
}


} // namespace reflect
} // namespace bee