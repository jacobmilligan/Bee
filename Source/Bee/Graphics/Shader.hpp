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


class BEE_REFLECT(serializable, version = 1) Shader
{
public:
    struct BEE_REFLECT(serializable) Range
    {
        i32 offset { -1 };
        i32 size { -1 };
    };

    struct BEE_REFLECT(serializable, version = 1) Pass
    {
        Range               attachments;
        Range               subpasses;

        BEE_REFLECT(ignored)
        RenderPassHandle    gpu_handle;
    };

    struct BEE_REFLECT(serializable, version = 1) Pipeline
    {
        PipelineStateCreateInfo     info;
        i32                         pass { -1 };
        i32                         shaders[gpu_shader_stage_count];

        BEE_REFLECT(ignored)
        PipelineStateHandle         gpu_handle;
    };

    struct BEE_REFLECT(serializable, version = 1) SubShader
    {
        StaticString<128>   name;
        StaticString<128>   stage_entries[gpu_shader_stage_count];
        Range               stage_code_ranges[gpu_shader_stage_count];

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
};



} // namespace bee