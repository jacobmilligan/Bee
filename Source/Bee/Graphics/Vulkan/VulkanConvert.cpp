/*
 *  VulkanConvert.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Graphics/Vulkan/VulkanConvert.hpp"
#include "Bee/Core/Meta.hpp"

namespace bee {


PhysicalDeviceType convert_device_type(VkPhysicalDeviceType type)
{
    switch (type)
    {
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return PhysicalDeviceType::other;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return PhysicalDeviceType::integrated;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return PhysicalDeviceType::discrete;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return PhysicalDeviceType::virtual_gpu;
        default:
            break;
    }
    return PhysicalDeviceType::unknown;
}

PhysicalDeviceVendor convert_vendor(const u32 id)
{
    switch (id)
    {
        case 0x1002:
            return PhysicalDeviceVendor::AMD;
        case 0x1010:
            return PhysicalDeviceVendor::ImgTec;
        case 0x10DE:
            return PhysicalDeviceVendor::NVIDIA;
        case 0x13B5:
            return PhysicalDeviceVendor::ARM;
        case 0x5143:
            return PhysicalDeviceVendor::Qualcomm;
        case 0x8086:
            return PhysicalDeviceVendor::Intel;
        default:
            break;
    }
    return PhysicalDeviceVendor::unknown;
}

VkColorComponentFlags decode_color_write_mask(const ColorWriteMask mask)
{
    return 0u
        | decode_flag(mask, ColorWriteMask::alpha, VK_COLOR_COMPONENT_A_BIT)
        | decode_flag(mask, ColorWriteMask::blue, VK_COLOR_COMPONENT_B_BIT)
        | decode_flag(mask, ColorWriteMask::green, VK_COLOR_COMPONENT_G_BIT)
        | decode_flag(mask, ColorWriteMask::red, VK_COLOR_COMPONENT_R_BIT);
}

VkBufferUsageFlags decode_buffer_type(const BufferType type)
{
    return 0u
        | decode_flag(type, BufferType::vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        | decode_flag(type, BufferType::index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
        | decode_flag(type, BufferType::uniform_buffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        | decode_flag(type, BufferType::transfer_dst, VK_BUFFER_USAGE_TRANSFER_DST_BIT)
        | decode_flag(type, BufferType::transfer_src, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

VkShaderStageFlags decode_shader_stage(const ShaderStage stages)
{
    auto vk_flags = 0u
        | decode_flag(stages, ShaderStage::vertex, VK_SHADER_STAGE_VERTEX_BIT)
        | decode_flag(stages, ShaderStage::fragment, VK_SHADER_STAGE_FRAGMENT_BIT)
        | decode_flag(stages, ShaderStage::geometry, VK_SHADER_STAGE_GEOMETRY_BIT)
        | decode_flag(stages, ShaderStage::compute, VK_SHADER_STAGE_COMPUTE_BIT);

    if (underlying_t(stages & ShaderStage::graphics) - 1 >= underlying_t(ShaderStage::graphics) - 1)
    {
        vk_flags |= VK_SHADER_STAGE_ALL_GRAPHICS;
    }

    if (underlying_t(stages & ShaderStage::all) - 1 >= underlying_t(ShaderStage::all) - 1)
    {
        vk_flags |= VK_SHADER_STAGE_ALL;
    }

    return vk_flags;
}

VkAccessFlags decode_buffer_access(const BufferType& type, const bool is_read)
{
    return 0u
        | decode_flag(type, BufferType::index_buffer, (is_read) ? VK_ACCESS_INDEX_READ_BIT : VK_ACCESS_MEMORY_WRITE_BIT)
        | decode_flag(type, BufferType::vertex_buffer, (is_read) ? VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT : VK_ACCESS_MEMORY_WRITE_BIT)
        | decode_flag(type, BufferType::uniform_buffer, (is_read) ? VK_ACCESS_UNIFORM_READ_BIT : VK_ACCESS_MEMORY_WRITE_BIT)
        | decode_flag(type, BufferType::transfer_dst, (is_read) ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT)
        | decode_flag(type, BufferType::transfer_src, (is_read) ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT);
}

VkImageUsageFlags decode_image_usage(const TextureUsage& usage)
{
    return 0u
        | decode_flag(usage, TextureUsage::transfer_src, VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        | decode_flag(usage, TextureUsage::transfer_dst, VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        | decode_flag(usage, TextureUsage::color_attachment, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        | decode_flag(usage, TextureUsage::depth_stencil_attachment, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        | decode_flag(usage, TextureUsage::sampled, VK_IMAGE_USAGE_SAMPLED_BIT)
        | decode_flag(usage, TextureUsage::storage, VK_IMAGE_USAGE_STORAGE_BIT)
        | decode_flag(usage, TextureUsage::input_attachment, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
}


} // namespace bee