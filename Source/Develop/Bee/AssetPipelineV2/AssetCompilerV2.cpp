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

static DynamicArray<AssetCompilerInfo> g_compilers;


AssetCompilerContext::AssetCompilerContext(const GUID& guid, const AssetPlatform platform, Allocator* allocator)
    : guid_(guid),
      platform_(platform),
      allocator_(allocator),
      artifacts_(allocator)
{}

void AssetCompilerContext::add_artifact(const Type* type, void* data)
{
    io::
}

u32 get_extension_hash(const StringView& ext)
{
    return ext[0] == '.' ? get_hash(StringView(ext.data() + 1, ext.size() - 1)) : get_hash(ext);
}

i32 find_compiler_by_type(const Type* type)
{
    return container_index_of(g_compilers, [&](const AssetCompilerInfo& info)
    {
        return info.type->hash == type->hash;
    });
}

i32 find_compiler_by_extension(const StringView& ext)
{
    const auto ext_hash = get_extension_hash(ext);
    for (auto info : enumerate(g_compilers))
    {
        if (container_index_of(info.value.extensions, [&](const u32 hash) { return hash == ext_hash; }) >= 0)
        {
            return info.index;
        }
    }
    return -1;
}

bool find_extension_mapping(const Type* type, const char* ext, i32* compiler_index, i32* ext_index)
{
    *compiler_index = find_compiler_by_type(type);
    if (*compiler_index < 0)
    {
        return false;
    }

    const auto ext_hash = get_extension_hash(ext);
    *ext_index = container_index_of(g_compilers[*compiler_index].extensions, [&](const u32 hash)
    {
        return hash == ext_hash;
    });
    return *ext_index >= 0;
}


void register_asset_compiler(const Type* type, AssetCompiler*(*allocate_function)())
{
    if (BEE_FAIL_F(current_thread::is_main(), "Asset compilers must be registered on the main thread"))
    {
        return;
    }

    // Validate unique name
    if (BEE_FAIL_F(find_compiler_by_type(type) < 0, "%s is already a registered asset compiler", type->name))
    {
        return;
    }

    AssetCompilerInfo info;
    info.type = type;

    // Validate that no compilers have been registered with the supported extensions
    for (const Attribute& attr : type->as<RecordType>()->attributes)
    {
        if (str::compare(attr.name, "ext") != 0 || attr.kind != AttributeKind::string)
        {
            continue;
        }

        auto ext = attr.value.string;
        if (BEE_FAIL_F(find_compiler_by_extension(ext) < 0, "Cannot register asset compiler: a compiler is already registered for file type \"%s\"", ext))
        {
            return;
        }

        info.extensions.push_back(get_extension_hash(ext));
    }

    for (int i = 0; i < get_job_worker_count(); ++i)
    {
        info.per_thread.push_back(allocate_function());
    }

    g_compilers.push_back(info);
}

void unregister_asset_compiler(const Type* type)
{
    if (BEE_FAIL_F(current_thread::is_main(), "Asset compilers must be unregistered on the main thread"))
    {
        return;
    }

    auto compiler = find_compiler_by_type(type);
    if (BEE_FAIL_F(compiler >= 0, "Cannot unregister asset compiler: no compiler registered with name \"%s\"", type->name))
    {
        return;
    }

    g_compilers.erase(compiler);
}

void asset_compiler_add_file_type(const Type* type, const char* extension)
{
    if (BEE_FAIL_F(current_thread::is_main(), "Asset compilers can only be modified on the main thread"))
    {
        return;
    }

    const auto compiler_idx = find_compiler_by_type(type);
    if (BEE_FAIL_F(compiler_idx < 0, "No asset compiler is registered with name \"%s\"", type->name))
    {
        return;
    }

    if (BEE_FAIL_F(find_compiler_by_extension(extension) < 0, "An asset compiler is already registered for file type with extension \"%s\"", extension))
    {
        return;
    }

    g_compilers[compiler_idx].extensions.push_back(get_extension_hash(extension));
}

void asset_compiler_remove_file_type(const Type* type, const char* extension)
{
    if (BEE_FAIL_F(current_thread::is_main(), "Asset compilers can only be modified on the main thread"))
    {
        return;
    }

    int compiler_idx = -1;
    int ext_idx = -1;
    if (!find_extension_mapping(type, extension, &compiler_idx, &ext_idx))
    {
        BEE_ASSERT_F(compiler_idx >= 0, "No asset compiler is registered with name \"%s\"", type->name);
        BEE_ASSERT_F(ext_idx >= 0, "Asset compiler \"%s\" does not support file type with extension \"%s\"", type->name, extension);
        return;
    }

    g_compilers[compiler_idx].extensions.erase(ext_idx);
}


struct CompileAssetJob final : public Job
{
    i32             compiler_index { -1 };
    AssetPlatform   platform { AssetPlatform::unknown };
    Path            path;

    explicit CompileAssetJob(const i32 compiler_index_to_use, const AssetPlatform platform_to_compile, const char* src_path)
        : compiler_index(compiler_index_to_use),
          platform(platform_to_compile),
          path(src_path, job_temp_allocator())
    {}

    void execute() override
    {
        auto compiler = g_compilers[compiler_index].per_thread[get_local_job_worker_id()];
        AssetCompilerContext ctx(platform, job_temp_allocator());
        compiler->compile(&ctx);
    }
};


bool compile_asset(JobGroup* group, const AssetPlatform platform, const char* path)
{
    const auto ext = path_get_extension(path);
    auto compiler = find_compiler_by_extension(ext);
    if (BEE_FAIL_F(compiler >= 0, "Failed to compile asset: no compiler registered for file type %" BEE_PRIsv, BEE_FMT_SV(ext)))
    {
        return false;
    }

    auto job = allocate_job<CompileAssetJob>(compiler, platform, path);
    job_schedule(group, job);
    return true;
}

bool compile_asset_sync(const AssetPlatform platform, const char* path)
{
    JobGroup group{};
    if (!compile_asset(&group, platform, path))
    {
        return false;
    }
    job_wait(&group);
    return true;
}


} // namespace bee