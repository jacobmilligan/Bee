/*
 *  Shader.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/ShaderPipeline/Cache.hpp"

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Serialization/Serialization.hpp"

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


struct BEE_REFLECT(serializable, version = 1) ShaderPipelineStage
{
    String                          entry;
    ShaderStageFlags                flags;

    BEE_REFLECT(bytes)
    FixedArray<u8>                  code;

    BEE_REFLECT(nonserialized)
    ShaderHandle                    shader_resource;

    StaticArray<ResourceBindingUpdateFrequency, BEE_GPU_MAX_RESOURCE_LAYOUTS> update_frequencies;
};

struct BEE_REFLECT(serializable, version = 1, use_builder) ShaderPipeline
{
    String                          name;
    PipelineStateCreateInfo         pipeline_info;
    RenderPassCreateInfo            render_pass_info;
    FixedArray<SubPassDescriptor>   subpasses;
    FixedArray<ShaderPipelineStage> stages;

    BEE_REFLECT(nonserialized)
    RenderPassHandle                render_pass_resource;

    BEE_REFLECT(nonserialized)
    PipelineStateHandle             pipeline_resource;
};


BEE_SERIALIZE_TYPE(SerializationBuilder* builder, ShaderPipeline* pipeline)
{
    // we can automatically serialize the pipeline as normal using the defined version info etc.
    serialize_type(builder->serializer(), builder->params());

    if (builder->mode() == SerializerMode::reading)
    {
        // ... but if we're in read mode we have to fixup the subpasses pointer
        pipeline->render_pass_info.subpasses = pipeline->subpasses.data();
    }
}


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/Resource.generated.inl"
#endif // BEE_ENABLE_REFLECTION