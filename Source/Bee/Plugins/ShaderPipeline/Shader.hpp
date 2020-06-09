/*
 *  Shader.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Serialization/StreamSerializer.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Graphics/GPU.hpp"


namespace bee {


struct BEE_REFLECT(serializable, version = 1) Shader
{
    struct BEE_REFLECT(serializable, version = 1) Range
    {
        i32 offset { -1 };
        i32 size { -1 };

        inline bool empty() const
        {
            return offset < 0 || size <= 0;
        }
    };

    struct BEE_REFLECT(serializable, version = 1) Pass
    {
        Range               attachments;
        Range               subpasses;

        BEE_REFLECT(ignored)
        RenderPassHandle    gpu_handle;
    };

    struct BEE_REFLECT(serializable, version = 1) Pipeline // NOLINT
    {
        StaticString<128>           name;
        PipelineStateCreateInfo     info; // contains everything except the renderpass and shader handles
        i32                         pass { -1 };
        i32                         shaders[gpu_shader_stage_count];

        BEE_REFLECT(ignored)
        PipelineStateHandle         gpu_handle;
    };

    struct BEE_REFLECT(serializable, version = 1) SubShader
    {
        StaticString<128>           name;
        StaticString<128>           stage_entries[gpu_shader_stage_count];
        Range                       stage_code_ranges[gpu_shader_stage_count];

        BEE_REFLECT(ignored)
        ShaderHandle        stage_handles[gpu_shader_stage_count];
    };

    DynamicArray<Pass>                      passes;
    DynamicArray<Pipeline>                  pipelines;
    DynamicArray<SubShader>                 subshaders;
    DynamicArray<AttachmentDescriptor>      attachments;
    DynamicArray<SubPassDescriptor>         subpasses;
    DynamicArray<u8>                        code;

    BEE_REFLECT(nonserialized)
    DeviceHandle                            gpu_device;

    explicit Shader(Allocator* allocator = system_allocator())
        : passes(allocator),
          pipelines(allocator),
          subshaders(allocator),
          attachments(allocator),
          subpasses(allocator),
          code(allocator)
    {}

    const Pass& add_pass(const i32 attachment_count, const i32 subpass_count)
    {
        passes.emplace_back();
        passes.back().attachments.offset = attachments.size();
        passes.back().attachments.size = attachment_count;
        passes.back().subpasses.offset = subpasses.size();
        passes.back().subpasses.size = subpass_count;

        attachments.append(attachment_count, AttachmentDescriptor{});
        subpasses.append(subpass_count, SubPassDescriptor{});

        return passes.back();
    }

    Range add_code(const u8* data, const i32 size)
    {
        Range range{};
        range.offset = code.size();
        range.size = size;

        code.append({ data, size });

        return range;
    }

    ShaderHandle get_shader(const i32 pipeline, const ShaderStageIndex stage)
    {
        BEE_ASSERT(pipeline < pipelines.size());

        const auto subshader = pipelines[pipeline].shaders[underlying_t(stage)];

        BEE_ASSERT(subshader < subshaders.size());

        return subshaders[subshader].stage_handles[underlying_t(stage)];
    }
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/Shader.generated.inl"
#endif // BEE_ENABLE_REFLECTION