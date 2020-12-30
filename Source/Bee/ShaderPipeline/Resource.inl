/*
 *  Shader.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ShaderPipeline/Cache.hpp"

#include "Bee/AssetCache/AssetCache.hpp"

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Serialization/Serialization.hpp"

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


struct BEE_REFLECT(serializable, version = 1) ShaderPipelineStage
{
    StaticString<256>               entry;
    ShaderStageFlags                flags;

    BEE_REFLECT(bytes)
    FixedArray<u8>                  code;

    BEE_REFLECT(nonserialized)
    ShaderHandle                    shader_resource;

    StaticArray<ResourceBindingUpdateFrequency, BEE_GPU_MAX_RESOURCE_LAYOUTS> update_frequencies;
};

struct BEE_REFLECT(serializable, version = 1, use_builder) ShaderPipeline
{
    u32                                                         name_hash { 0 };
    PipelineStateDescriptor                                     pipeline_desc;
    StaticArray<ShaderPipelineStage, ShaderStageIndex::count>   stages;
};

struct ShaderCache
{
    RecursiveMutex                                              mutex;
    DynamicHashMap<u32, ShaderPipelineHandle>                   lookup;
    ResourcePool<ShaderPipelineHandle, ShaderPipeline>          pool;

    const GpuBackend*                                           gpu { nullptr };
    DeviceHandle                                                device;

    ShaderCache()
        : pool(sizeof(ShaderPipeline) * 64)
    {}
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/Resource.generated.inl"
#endif // BEE_ENABLE_REFLECTION