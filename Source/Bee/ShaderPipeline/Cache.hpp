/*
 *  Shader.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


struct Serializer;

struct ShaderPipelineStageResourceDescriptor
{
    i32                             layout_count { 0 };
    ResourceBindingUpdateFrequency  frequencies[BEE_GPU_MAX_RESOURCE_LAYOUTS];
};

struct ShaderPipelineDescriptor
{
    const char*                             name { nullptr };
    PipelineStateCreateInfo                 create_info;
    RenderPassCreateInfo                    render_pass_info;

    i32                                     shader_stage_count { 0 };
    ShaderCreateInfo                        shader_info[ShaderStageIndex::count];
    ShaderStageFlags                        shader_stages[ShaderStageIndex::count];
    ShaderPipelineStageResourceDescriptor   shader_resources[ShaderStageIndex::count];
};

#define BEE_SHADER_MODULE "BEE_SHADER"

BEE_VERSIONED_HANDLE_32(ShaderPipelineHandle);

struct ShaderModule
{
    RenderPassHandle (*get_render_pass)(const ShaderPipelineHandle shader) { nullptr };

    PipelineStateHandle (*get_pipeline_state)(const ShaderPipelineHandle shader) { nullptr };

    ShaderHandle (*get_stage)(const ShaderPipelineHandle, const ShaderStageIndex stage) { nullptr };

    void (*load)(const ShaderPipelineHandle handle) { nullptr };

    void (*unload)(const ShaderPipelineHandle handle) { nullptr };
};

#define BEE_SHADER_CACHE_MODULE_NAME "BEE_SHADER_CACHE"

struct ShaderCache;
struct ShaderCacheModule
{
    ShaderCache* (*create)() { nullptr };

    void (*destroy)(ShaderCache* cache) { nullptr };

    void (*load)(ShaderCache* cache, Serializer* serializer) { nullptr };

    void (*save)(ShaderCache* cache, Serializer* serializer) { nullptr };

    ShaderPipelineHandle (*add_shader)(ShaderCache* cache, const ShaderPipelineDescriptor& desc) { nullptr };

    void (*remove_shader)(ShaderCache* cache, const ShaderPipelineHandle handle) { nullptr };

    ShaderPipelineHandle (*get_shader)(ShaderCache* cache, const u32 name_hash) { nullptr };

    u32 (*get_shader_hash)(ShaderCache* cache, const ShaderPipelineHandle handle) { nullptr };

    u32 (*get_shader_name_hash)(const StringView& name) { nullptr };

    template <i32 Size>
    ShaderPipelineHandle get_shader_by_name(ShaderCache* cache, char(&name)[Size])
    {
        return get_shader(get_static_string_hash(name));
    }

    inline ShaderPipelineHandle get_shader_by_name(ShaderCache* cache, const StringView& name)
    {
        return get_shader(cache, get_shader_name_hash(name));
    }
};


} // namespace bee