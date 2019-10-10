/*
 *  VulkanConvert.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/GPU.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>


namespace bee {



inline constexpr bool vkbool_cast(const VkBool32 value)
{
    return static_cast<bool>(value);
}

PhysicalDeviceType convert_device_type(VkPhysicalDeviceType type);

PhysicalDeviceVendor convert_vendor(const u32 id);


VkColorComponentFlags decode_color_write_mask(const ColorWriteMask mask);

VkBufferUsageFlags decode_buffer_type(const BufferType type);

VkShaderStageFlags decode_shader_stage(const ShaderStage stages);

VkAccessFlags decode_buffer_access(const BufferType& type, const bool is_read);

VkImageUsageFlags decode_image_usage(const TextureUsage& usage);

VkImageAspectFlags select_aspect_mask(const PixelFormat format);

VkFormat convert_pixel_format(const PixelFormat value);

VkAttachmentLoadOp convert_load_op(LoadOp value);

VkAttachmentStoreOp convert_store_op(StoreOp value);

VkVertexInputRate convert_step_function(StepFunction value);

VkFormat convert_vertex_format(VertexFormat value);

VkPrimitiveTopology convert_primitive_type(PrimitiveType value);

VkPolygonMode convert_polygon_mode(FillMode value);

VkCullModeFlagBits convert_cull_mode(CullMode value);

VkBlendFactor convert_blend_factor(BlendFactor value);

VkBlendOp convert_blend_op(BlendOperation value);

VkDescriptorType convert_descriptor_type(ResourceType value);

VmaMemoryUsage convert_memory_usage(DeviceMemoryUsage value);

VkCompareOp convert_compare_op(CompareFunc value);

VkStencilOp convert_stencil_op(StencilOp value);

VkImageType convert_image_type(TextureType value);

VkImageViewType convert_image_view_type(TextureType value);

VkFilter convert_filter(MinMagFilter value);

VkSamplerMipmapMode convert_mip_map_mode(MipMapMode value);

VkSamplerAddressMode convert_address_mode(AddressMode value);

VkBorderColor convert_border_color(BorderColor value);



} // namespace bee