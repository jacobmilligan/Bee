/*
 *  ShaderLoader.hpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


using ResourceBindingHandleArray = StaticArray<ResourceBindingHandle, BEE_GPU_MAX_RESOURCE_LAYOUTS>;

struct BEE_REFLECT(serializable, version = 1) ShaderSampler
{
    SamplerCreateInfo   info;
    u32                 hash { 0 }; // used as a shortcut for searching etc.
    u32                 binding { 0 };
    u32                 layout { 0 };

    BEE_REFLECT(ignored)
    SamplerHandle       handle;
};

struct BEE_REFLECT(serializable, version = 1) ShaderStage
{
    StaticString<256>               entry;
    BEE_REFLECT(bytes)
    FixedArray<u8>                  code;
    ShaderStageFlags                flags;

    BEE_PAD(4);

    BEE_REFLECT(ignored)
    ShaderHandle                    shader_resource;

    ShaderStage(Allocator* allocator = system_allocator())
        : code(allocator)
    {}
};

struct BEE_REFLECT(serializable, version = 1) Shader
{
    StaticString<256>                                   name;
    PipelineStateDescriptor                             pipeline_desc;
    StaticArray<ShaderStage, ShaderStageIndex::count>   stages;
    ResourceBindingUpdateFrequencyArray                 update_frequencies;
    FixedArray<ShaderSampler>                           samplers;

    BEE_REFLECT(ignored)
    ResourceBindingHandleArray                          resource_bindings;

    Shader(Allocator* allocator = system_allocator())
    {
        for (int i = 0; i < stages.capacity; ++i)
        {
            new (&stages[i]) ShaderStage(allocator);
        }
    }
};

struct BEE_REFLECT(serializable, version = 1) ShaderImportSettings
{
    bool output_debug_shaders { false };
};

#define BEE_SHADER_PIPELINE_MODULE_NAME "BEE_SHADER_PIPELINE"

struct AssetPipeline;
struct ShaderPipelineModule
{
    void (*init)(AssetPipeline* asset_pipeline, GpuBackend* gpu, const DeviceHandle device) { nullptr };

    void (*shutdown)() { nullptr };

    bool (*load_shader)(Shader* shader) { nullptr };

    void (*unload_shader)(Shader* shader) { nullptr };

    void (*update_resources)(Shader* shader, const u32 layout, const i32 count, const ResourceBindingUpdate* updates) { nullptr };

    void (*bind_resource_layout)(Shader* shader, CommandBuffer* cmd_buf, const u32 layout) { nullptr };

    void (*bind_resources)(Shader* shader, CommandBuffer* cmd_buf) { nullptr };
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/ShaderPipeline.generated.inl"
#endif // BEE_ENABLE_REFLECTION
