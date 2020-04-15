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
#include "Bee/Core/Jobs/JobSystem.hpp"

#include <algorithm>

namespace bee {


i32 max_threads()
{
    const auto count = get_job_worker_count();

    if (BEE_FAIL_F(count > 0, "Cannot get max_threads value until job system is initialized"))
    {
        return 0;
    }

    return count;
}


AssetCompilerContext::AssetCompilerContext(const AssetPlatform platform, const StringView& location, const StringView& cache_dir, const TypeInstance& options, Allocator* allocator)
    : platform_(platform),
      location_(location),
      cache_dir_(cache_dir),
      options_(options),
      allocator_(allocator),
      artifacts_(allocator)
{}

void AssetCompilerContext::add_artifact(const size_t size, const void* data)
{
    AssetArtifact artifact{};
    artifact.buffer_size = size;
    artifact.buffer = static_cast<const u8*>(data);
    artifacts_.push_back(artifact);
}

void AssetCompilerContext::calculate_hashes()
{
    for (auto& artifact : artifacts_)
    {
        artifact.hash = get_hash128(artifact.buffer, artifact.buffer_size, 0xF00D);
    }

    std::sort(artifacts_.begin(), artifacts_.end(), [&](const AssetArtifact& lhs, const AssetArtifact& rhs)
    {
        return lhs.hash < rhs.hash;
    });
}

u32 get_extension_hash(const StringView& ext)
{
    return ext[0] == '.' ? get_hash(StringView(ext.data() + 1, ext.size() - 1)) : get_hash(ext);
}

AssetCompilerRegistry::~AssetCompilerRegistry()
{
    clear();
}

i32 AssetCompilerRegistry::find_compiler(const u32 hash)
{
    return container_index_of(compilers_, [&](const CompilerInfo& info)
    {
        return info.type->hash == hash;
    });
}

void AssetCompilerRegistry::register_compiler(AssetCompiler* compiler, const Type* type)
{
    if (find_compiler(type->hash) >= 0)
    {
        log_error("Asset compiler \"%s\" is already registered", type->name);
        return;
    }

    compilers_.emplace_back();

    auto& info = compilers_.back();
    info.type = type;
    info.options_type = get_type<UnknownType>();
    info.compiler = compiler;

    const auto compiler_id = compilers_.size() - 1;

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

        filetype_mapping->value.compiler_ids.push_back(compiler_id);
        filetype_mapping->value.compiler_hashes.push_back(type->hash);

        info.extensions.push_back(ext_hash);
    }

    info.compiler->init(max_threads());
}

void AssetCompilerRegistry::unregister_compiler(const Type* type)
{
    auto id = find_compiler(type->hash);
    if (BEE_FAIL_F(id >= 0, "Cannot unregister asset compiler: no compiler registered with name \"%s\"", type->name))
    {
        return;
    }

    for (const auto hash : compilers_[id].extensions)
    {
        auto extension_mapping = filetype_map_.find(hash);
        if (extension_mapping != nullptr)
        {
            const auto compiler_mapping_idx = container_index_of(extension_mapping->value.compiler_ids, [&](const i32 stored)
            {
                return stored == id;
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

    compilers_[id].compiler->destroy();
    compilers_.erase(id);
}

AssetCompilerStatus AssetCompilerRegistry::compile(AssetCompilerContext* ctx)
{
    const auto ext_hash = get_hash(path_get_extension(ctx->location()));
    auto filetype = filetype_map_.find(ext_hash);
    if (filetype == nullptr)
    {
        return AssetCompilerStatus::unsupported_filetype;
    }

    auto status = AssetCompilerStatus::unknown;

    for (auto& id : filetype->value.compiler_ids)
    {
        auto compiler = compilers_[id].compiler;

        status = compiler->compile(get_local_job_worker_id(), ctx);

        if (status != AssetCompilerStatus::success)
        {
            break;
        }
    }

    ctx->calculate_hashes();
    return status;
}

void AssetCompilerRegistry::clear()
{
    compilers_.clear();
    filetype_map_.clear();
}


} // namespace bee