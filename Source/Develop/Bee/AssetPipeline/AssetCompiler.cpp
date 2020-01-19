/*
 *  AssetCompiler.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"

namespace bee {


BEE_TRANSLATION_TABLE(asset_compiler_status_to_string, AssetCompilerStatus, const char*, AssetCompilerStatus::unknown,
    "success",                  // success
    "fatal_error",              // fatal_error
    "unsupported_platform",     // unsupported_platform
    "invalid_source_format"     // invalid_source_format
)


struct AssetCompilerInfo
{
    const Type*                     type { nullptr };
    const Type*                     options_type { nullptr };
    DynamicArray<u32>               extensions;
    DynamicArray<AssetCompiler*>    per_thread;
};

struct AssetFileType
{
    String                extension;
    DynamicArray<i32>     compiler_ids;
    DynamicArray<u32>     compiler_hashes;
};

static DynamicArray<AssetCompilerInfo>      g_compilers;
static DynamicHashMap<u32, AssetFileType>   g_filetype_map;


AssetCompilerContext::AssetCompilerContext(const AssetPlatform platform, const StringView& location, const TypeInstance& options, Allocator* allocator)
    : platform_(platform),
      location_(location),
      options_(options),
      allocator_(allocator),
      artifacts_(allocator)
{}

io::MemoryStream AssetCompilerContext::add_artifact()
{
    artifacts_.emplace_back(allocator_);
    return io::MemoryStream(&artifacts_.back().buffer);
}

void AssetCompilerContext::calculate_hashes()
{
    for (auto& artifact : artifacts_)
    {
        artifact.hash = get_hash128(artifact.buffer.data(), artifact.buffer.size(), 0xF00D);
    }
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
    info.options_type = get_type<UnknownType>();

    const auto compiler_id = g_compilers.size() - 1;

    // Validate that no compilers have been registered with the supported extensions
    for (const Attribute& attr : type->as<RecordType>()->attributes)
    {
        // get the options type attribute
        if (str::compare(attr.name, "options") == 0 && attr.kind == AttributeKind::type)
        {
            if (BEE_CHECK_F(info.options_type->is(TypeKind::unknown), "Asset compiler defines more than one options type"))
            {
                info.options_type = attr.value.type;
            }
            continue;
        }

        // Handle supported file type extensions
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


        if (kind == AssetCompilerKind::default_compiler)
        {
            // default compiler always comes first
            filetype_mapping->value.compiler_ids.insert(0, compiler_id);
            filetype_mapping->value.compiler_hashes.insert(0, type->hash);
        }
        else
        {
            filetype_mapping->value.compiler_ids.push_back(compiler_id);
            filetype_mapping->value.compiler_hashes.push_back(type->hash);
        }

        info.extensions.push_back(ext_hash);
    }

    // One pool allocator for options per thread - we'll lock
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
            const auto compiler_mapping_idx = container_index_of(extension_mapping->value.compiler_ids, [&](const i32 index)
            {
                return index == compiler_index;
            });

            if (compiler_mapping_idx >= 0)
            {
                extension_mapping->value.compiler_ids.erase(compiler_mapping_idx);
                extension_mapping->value.compiler_hashes.erase(compiler_mapping_idx);

                if (extension_mapping->value.compiler_ids.empty())
                {
                    g_filetype_map.erase(hash);
                }
            }
        }
    }

    g_compilers.erase(compiler_index);
}

Span<const i32> get_asset_compiler_ids(const StringView& path)
{
    const auto ext_hash = get_extension_hash(path_get_extension(path));
    const auto file_type_mapping = g_filetype_map.find(ext_hash);
    if (file_type_mapping == nullptr)
    {
        return Span<const i32>{};
    }

    return file_type_mapping->value.compiler_ids.const_span();
}

Span<const u32> get_asset_compiler_hashes(const StringView& path)
{
    const auto ext_hash = get_extension_hash(path_get_extension(path));
    const auto file_type_mapping = g_filetype_map.find(ext_hash);
    if (file_type_mapping == nullptr)
    {
        return Span<const u32>{};
    }

    return file_type_mapping->value.compiler_hashes.const_span();
}

AssetCompiler* get_default_asset_compiler(const StringView& path)
{
    const auto compiler_ids = get_asset_compiler_ids(path);
    return compiler_ids.empty() ? nullptr : g_compilers[compiler_ids[0]].per_thread[get_local_job_worker_id()];
}

AssetCompiler* get_asset_compiler(const i32 id)
{
    if (id < 0 || id >= g_compilers.size())
    {
        return nullptr;
    }

    return g_compilers[id].per_thread[get_local_job_worker_id()];
}

AssetCompiler* get_asset_compiler(const u32 hash)
{
    const auto compiler_index = find_compiler(hash);
    return compiler_index < 0 ? nullptr : g_compilers[compiler_index].per_thread[get_local_job_worker_id()];
}

const Type* get_asset_compiler_options_type(const u32 compiler_hash)
{
    const auto compiler = find_compiler(compiler_hash);
    return compiler >= 0 ? g_compilers[compiler].options_type : get_type<UnknownType>();
}




} // namespace bee