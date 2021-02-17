/*
 *  Shader.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ShaderPipeline/Cache.hpp"

#include "Bee/AssetPipelineV2/AssetPipeline.hpp"

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Serialization/Serialization.hpp"

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


using ResourceUpdateFrequencyArray = StaticArray<ResourceBindingUpdateFrequency, BEE_GPU_MAX_RESOURCE_LAYOUTS>;
using ResourceBindingHandleArray = StaticArray<ResourceBindingHandle, BEE_GPU_MAX_RESOURCE_LAYOUTS>;

struct BEE_REFLECT(serializable, version = 1) ShaderPipelineStage
{
    StaticString<256>               entry;
    ShaderStageFlags                flags;
    BEE_REFLECT(bytes)
    FixedArray<u8>                  code;
    ResourceUpdateFrequencyArray    update_frequencies;

    BEE_REFLECT(ignored)
    ShaderHandle                    shader_resource;
};

struct BEE_REFLECT(serializable, version = 1) ShaderPipeline
{
    GUID                                                        guid;
    u32                                                         name_hash { 0 };
    PipelineStateDescriptor                                     pipeline_desc;
    StaticArray<ShaderPipelineStage, ShaderStageIndex::count>   stages;

    BEE_REFLECT(ignored)
    ResourceBindingHandleArray                                  resource_bindings;
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