/*
 *  GpuObject.hpp.h
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Handle.hpp"

namespace bee {


/*
 ********************************************************
 *
 * # GPU Handles
 *
 * Handles to GPU objects. Most of these are versioned
 * integer handles used to index into an array in the
 * backend.
 *
 ********************************************************
 */
enum class GpuObjectType
{
    texture,
    texture_view,
    buffer,
    buffer_view,
    render_pass,
    shader,
    pipeline_state,
    fence,
    resource_binding,
    sampler,
    count
};

BEE_SPLIT_HANDLE(GpuObjectHandle, u64, 32u, 32u, value, thread);

#define BEE_GPU_HANDLE(NAME, T)                                                         \
    struct NAME                                                                         \
    {                                                                                   \
        static constexpr GpuObjectType type = GpuObjectType::T;                         \
        BEE_SPLIT_HANDLE_BODY(NAME, u64, 32u, 32u, value, thread)                       \
        inline constexpr operator GpuObjectHandle() { return GpuObjectHandle(id); }     \
        constexpr NAME(const GpuObjectHandle obj) : id(obj.id) {}                       \
    }

BEE_RAW_HANDLE_U32(DeviceHandle);
BEE_RAW_HANDLE_U32(SwapchainHandle);
BEE_GPU_HANDLE(TextureHandle, texture);
BEE_GPU_HANDLE(TextureViewHandle, texture_view);
BEE_GPU_HANDLE(BufferHandle, buffer);
BEE_GPU_HANDLE(BufferViewHandle, buffer_view);
BEE_GPU_HANDLE(RenderPassHandle, render_pass);
BEE_GPU_HANDLE(ShaderHandle, shader);
BEE_GPU_HANDLE(FenceHandle, fence);
BEE_GPU_HANDLE(SamplerHandle, sampler);
BEE_GPU_HANDLE(ResourceBindingHandle, resource_binding);
struct CommandBuffer;


} // namespace bee