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

AssetCompilerId AssetCompilerPipeline::find_compiler(const u32 hash)
{
    const auto index = container_index_of(compilers_, [&](const CompilerInfo& info)
    {
        return info.type->hash == hash;
    });

    return AssetCompilerId(index);
}

void AssetCompilerPipeline::register_compiler(const AssetCompilerKind kind, const Type* type, UniquePtr<AssetCompiler>&& compiler)
{
    scoped_rw_write_lock_t lock(mutex_);

    compilers_.emplace_back();

    auto& info = compilers_.back();
    info.type = type;
    info.options_type = get_type<UnknownType>();

    const AssetCompilerId compiler_id(compilers_.size() - 1);

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

        auto filetype_mapping = filetype_map_.find(ext_hash);
        if (filetype_mapping == nullptr)
        {
            filetype_mapping = filetype_map_.insert(ext_hash, FileTypeMapping());
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
}

void AssetCompilerPipeline::unregister_compiler(const Type* type)
{
    if (BEE_FAIL_F(current_thread::is_main(), "Asset compilers must be unregistered on the main thread"))
    {
        return;
    }

    auto id = find_compiler(type->hash);
    if (BEE_FAIL_F(id.is_valid(), "Cannot unregister asset compiler: no compiler registered with name \"%s\"", type->name))
    {
        return;
    }

    for (const auto hash : compilers_[id.id].extensions)
    {
        auto extension_mapping = filetype_map_.find(hash);
        if (extension_mapping != nullptr)
        {
            const auto compiler_mapping_idx = container_index_of(extension_mapping->value.compiler_ids, [&](const AssetCompilerId index)
            {
                return index == id;
            });

            if (compiler_mapping_idx >= 0)
            {
                extension_mapping->value.compiler_ids.erase(compiler_mapping_idx);
                extension_mapping->value.compiler_hashes.erase(compiler_mapping_idx);

                if (extension_mapping->value.compiler_ids.empty())
                {
                    filetype_map_.erase(hash);
                }
            }
        }
    }

    compilers_.erase(id.id);
}

Span<const AssetCompilerId> AssetCompilerPipeline::get_compiler_ids(const StringView& path)
{
    const auto ext_hash = get_extension_hash(path_get_extension(path));
    const auto file_type_mapping = filetype_map_.find(ext_hash);
    if (file_type_mapping == nullptr)
    {
        return Span<const AssetCompilerId>{};
    }

    return file_type_mapping->value.compiler_ids.const_span();
}

Span<const u32> AssetCompilerPipeline::get_compiler_hashes(const StringView& path)
{
    const auto ext_hash = get_extension_hash(path_get_extension(path));
    const auto file_type_mapping = filetype_map_.find(ext_hash);
    if (file_type_mapping == nullptr)
    {
        return Span<const u32>{};
    }

    return file_type_mapping->value.compiler_hashes.const_span();
}

AssetCompiler* AssetCompilerPipeline::get_default_compiler(const StringView& path)
{
    const auto compiler_ids = get_compiler_ids(path);
    return compiler_ids.empty() ? nullptr : compilers_[compiler_ids[0].id].compiler.get();
}

AssetCompiler* AssetCompilerPipeline::get_compiler(const AssetCompilerId& id)
{
    if (id.is_valid() || id.id >= compilers_.size())
    {
        return nullptr;
    }

    return compilers_[id.id].compiler.get();
}

AssetCompiler* AssetCompilerPipeline::get_compiler(const u32 hash)
{
    const auto id = find_compiler(hash);
    return !id.is_valid() ? nullptr : compilers_[id.id].compiler.get();
}

const Type* AssetCompilerPipeline::get_options_type(const AssetCompilerId& id)
{
    if (id.is_valid() || id.id >= compilers_.size())
    {
        return get_type<UnknownType>();
    }

    return compilers_[id.id].options_type;
}

const Type* AssetCompilerPipeline::get_options_type(const u32 hash)
{
    const auto id = find_compiler(hash);
    return !id.is_valid() ? get_type<UnknownType>() : compilers_[id.id].options_type;
}


} // namespace bee