/*
 *  Shader.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


BEE_VERSIONED_HANDLE_32(ShaderPipelineHandle);

struct Serializer;

struct ShaderPipelineStageResourceDescriptor
{
    i32                             layout_count { 0 };
    ResourceBindingUpdateFrequency  frequencies[BEE_GPU_MAX_RESOURCE_LAYOUTS];
};

struct ShaderPipelineDescriptor
{
    const char*                             name { nullptr };
    PipelineStateDescriptor                 pipeline;

    i32                                     shader_stage_count { 0 };
    ShaderCreateInfo                        shader_info[ShaderStageIndex::count];
    ShaderStageFlags                        shader_stages[ShaderStageIndex::count];
    ShaderPipelineStageResourceDescriptor   shader_resources[ShaderStageIndex::count];
};

struct ShaderAsset
{
    FixedArray<ShaderPipelineHandle> pipelines;
};

#define BEE_SHADER_CACHE_MODULE_NAME "BEE_SHADER_CACHE"

struct AssetCache;
struct ShaderPipeline;
struct ShaderCache;
struct ShaderCacheModule
{
    ShaderCache* (*create)() { nullptr };

    void (*destroy)(ShaderCache* cache) { nullptr };

    void (*load)(ShaderCache* cache, Serializer* serializer) { nullptr };

    void (*save)(ShaderCache* cache, Serializer* serializer) { nullptr };

    ShaderPipelineHandle (*add_shader)(ShaderCache* cache, const ShaderPipelineDescriptor& desc) { nullptr };

    void (*remove_shader)(ShaderCache* cache, const ShaderPipelineHandle handle) { nullptr };

    ShaderPipelineHandle (*lookup_shader)(ShaderCache* cache, const u32 name_hash) { nullptr };

    u32 (*get_shader_hash)(ShaderCache* cache, const ShaderPipelineHandle handle) { nullptr };

    u32 (*get_shader_name_hash)(const StringView& name) { nullptr };

    void (*register_asset_loader)(ShaderCache* shader_cache, AssetCache* asset_cache, const GpuBackend* gpu, const DeviceHandle device) { nullptr };

    void (*unregister_asset_loader)(ShaderCache* shader_cache, AssetCache* asset_cache) { nullptr };

    template <i32 Size>
    ShaderPipelineHandle get_shader_by_name(ShaderCache* cache, char(&name)[Size])
    {
        return lookup_shader(get_static_string_hash(name));
    }

    inline ShaderPipelineHandle get_shader_by_name(ShaderCache* cache, const StringView& name)
    {
        return lookup_shader(cache, get_shader_name_hash(name));
    }
};


} // namespace bee