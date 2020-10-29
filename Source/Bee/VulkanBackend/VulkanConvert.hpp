/*
 *  VulkanConvert.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Gpu/Gpu.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>


namespace bee {



inline constexpr bool vkbool_cast(const VkBool32 value)
{
    return static_cast<bool>(value);
}

inline constexpr VkRect2D vkrect2d_cast(const RenderRect& rect)
{
    return VkRect2D { { rect.x_offset, rect.y_offset }, { rect.width, rect.height } };
}

PhysicalDeviceType convert_device_type(VkPhysicalDeviceType type);

PhysicalDeviceVendor convert_vendor(const u32 id);

VkColorComponentFlags decode_color_write_mask(const ColorWriteMask mask);

VkBufferUsageFlags decode_buffer_type(const BufferType type);

VkShaderStageFlags decode_shader_stage(const ShaderStageFlags stages);

VkAccessFlags decode_buffer_access(const BufferType& type, const bool is_read);

VkImageUsageFlags decode_image_usage(const TextureUsage& usage);

VkSampleCountFlagBits decode_sample_count(const u32 samples);

VkImageAspectFlags select_access_mask_from_format(const PixelFormat format);

VkPipelineStageFlags select_pipeline_stage_from_access(const VkAccessFlags access);

VkFormat convert_pixel_format(const PixelFormat value);

VkAttachmentLoadOp convert_load_op(const LoadOp value);

VkAttachmentStoreOp convert_store_op(const StoreOp value);

VkVertexInputRate convert_step_function(const StepFunction value);

VkFormat convert_vertex_format(const VertexFormat value);

VkPrimitiveTopology convert_primitive_type(const PrimitiveType value);

VkPolygonMode convert_fill_mode(const FillMode value);

VkCullModeFlagBits convert_cull_mode(const CullMode value);

VkBlendFactor convert_blend_factor(const BlendFactor value);

VkBlendOp convert_blend_op(const BlendOperation value);

VkDescriptorType convert_resource_binding_type(const ResourceBindingType value);

VmaMemoryUsage convert_memory_usage(const DeviceMemoryUsage value);

VkCompareOp convert_compare_func(const CompareFunc value);

VkStencilOp convert_stencil_op(const StencilOp value);

VkImageType convert_image_type(const TextureType value);

VkImageViewType convert_image_view_type(const TextureType value);

VkFilter convert_filter(const MinMagFilter value);

VkSamplerMipmapMode convert_mip_map_mode(const MipMapMode value);

VkSamplerAddressMode convert_address_mode(const AddressMode value);

VkBorderColor convert_border_color(const BorderColor value);

VkCommandPoolCreateFlags convert_command_pool_hint(const CommandPoolHint hint);

VkCommandBufferResetFlags convert_command_buffer_reset_hint(const CommandStreamReset hint);

VkCommandBufferUsageFlags convert_command_buffer_usage(const CommandBufferUsage usage);

VkAccessFlags convert_access_mask(const GpuResourceState state);

VkImageLayout convert_image_layout(const GpuResourceState state);

VkIndexType convert_index_type(const IndexFormat format);


} // namespace bee