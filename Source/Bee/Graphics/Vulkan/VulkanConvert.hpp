/*
 *  VulkanConvert.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/GPU.hpp"

#include <volk.h>


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


} // namespace bee