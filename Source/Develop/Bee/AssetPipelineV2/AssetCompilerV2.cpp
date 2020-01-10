/*
 *  AssetCompilerV2.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipelineV2/AssetCompilerV2.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Thread.hpp"

namespace bee {


struct AssetCompilerInfo
{
    const Type*                     type { nullptr };
    DynamicArray<u32>               extensions;
    DynamicArray<AssetCompiler*>    per_thread;
};

struct AssetFileType
{
    String              extension;
    DynamicArray<i32>   compiler_ids;
};

static DynamicArray<AssetCompilerInfo>      g_compilers;
static DynamicHashMap<u32, AssetFileType>   g_filetype_map;


AssetCompilerContext::AssetCompilerContext(const AssetPlatform platform, Allocator* allocator)
    : platform_(platform),
      allocator_(allocator),
      artifacts_(allocator)
{}

io::MemoryStream AssetCompilerContext::add_artifact()
{
    artifacts_.emplace_back(allocator_);
    return io::MemoryStream(&artifacts_.back());
}

u32 get_extension_hash(const StringView& ext)
{
    return ext[0] == '.' ? get_hash(StringView(ext.data() + 1, ext.size() - 1)) : get_hash(ext);
}

i32 find_compiler(const u32 hash)
{
    return container_index_of(g_compilers, [&](const AssetCompilerInfo& info)
    {
        return info.type->hash == hash;
    });
}

void register_asset_compiler(const AssetCompilerKind kind, const Type* type, AssetCompiler*(*allocate_function)())
{
    if (BEE_FAIL_F(current_thread::is_main(), "Asset compilers must be registered on the main thread"))
    {
        return;
    }

    // Validate unique compiler
    if (BEE_FAIL_F(find_compiler(type->hash) < 0, "%s is already a registered asset compiler", type->name))
    {
        return;
    }

    g_compilers.emplace_back();

    auto& info = g_compilers.back();
    info.type = type;

    // Validate that no compilers have been registered with the supported extensions
    for (const Attribute& attr : type->as<RecordType>()->attributes)
    {
        if (str::compare(attr.name, "ext") != 0 || attr.kind != AttributeKind::string)
        {
            continue;
        }

        const auto ext = attr.value.string;
        const auto ext_hash = get_extension_hash(ext);

        if (container_index_of(info.extensions, [&](const u32 hash) { return hash == ext_hash; }) >= 0)
        {
            log_warning("Asset compiler \"%s\" defines the same file extension (%s) multiple times", type->name, ext);
            continue;
        }

        auto filetype_mapping = g_filetype_map.find(ext_hash);
        if (filetype_mapping == nullptr)
        {
            filetype_mapping = g_filetype_map.insert(ext_hash, AssetFileType());
            filetype_mapping->value.extension = ext;
        }

        const auto compiler_id = g_compilers.size() - 1;

        if (kind == AssetCompilerKind::default_compiler)
        {
            // default compiler always comes first
            filetype_mapping->value.compiler_ids.insert(0, compiler_id);
        }
        else
        {
            filetype_mapping->value.compiler_ids.push_back(compiler_id);
        }

        info.extensions.push_back(ext_hash);
    }

    for (int i = 0; i < get_job_worker_count(); ++i)
    {
        info.per_thread.push_back(allocate_function());
    }
}

void unregister_asset_compiler(const Type* type)
{
    if (BEE_FAIL_F(current_thread::is_main(), "Asset compilers must be unregistered on the main thread"))
    {
        return;
    }

    auto compiler_index = find_compiler(type->hash);
    if (BEE_FAIL_F(compiler_index >= 0, "Cannot unregister asset compiler: no compiler registered with name \"%s\"", type->name))
    {
        return;
    }

    for (const auto hash : g_compilers[compiler_index].extensions)
    {
        auto extension_mapping = g_filetype_map.find(hash);
        if (extension_mapping != nullptr)
        {
            const auto compiler_id = container_index_of(extension_mapping->value.compiler_ids, [&](const i32 index)
            {
                return index == compiler_index;
            });

            if (compiler_id >= 0)
            {
                extension_mapping->value.compiler_ids.erase(compiler_id);

                if (extension_mapping->value.compiler_ids.empty())
                {
                    g_filetype_map.erase(hash);
                }
            }
        }
    }

    g_compilers.erase(compiler_index);
}

AssetCompiler* get_default_asset_compiler(const char* path)
{
    const auto ext = path_get_extension(path);
    const auto hash = get_extension_hash(ext);
    auto filetype_mapping = g_filetype_map.find(hash);

    if (filetype_mapping == nullptr)
    {
        return nullptr;
    }

    BEE_ASSERT(!filetype_mapping->value.compiler_ids.empty());

    const auto compiler_index = filetype_mapping->value.compiler_ids[0];

    BEE_ASSERT(compiler_index < g_compilers.size());

    return g_compilers[compiler_index].per_thread[get_local_job_worker_id()];
}

AssetCompiler* get_asset_compiler(const u32 hash)
{
    const auto compiler_index = find_compiler(hash);
    if (compiler_index < 0)
    {
        return nullptr;
    }

    return g_compilers[compiler_index].per_thread[get_local_job_worker_id()];
}

AssetCompiler* get_asset_compiler(const Type* type)
{
    return get_asset_compiler(type->hash);
}



} // namespace bee