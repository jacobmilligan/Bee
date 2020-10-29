/*
 *  VulkanCache.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/VulkanBackend/VulkanObjectCache.hpp"
#include "Bee/VulkanBackend/VulkanDevice.hpp"
#include "Bee/VulkanBackend/VulkanConvert.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


void create_descriptor_set_layout(VulkanDevice* device, const ResourceLayoutDescriptor& key, VkDescriptorSetLayout* layout)
{
    auto bindings = FixedArray<VkDescriptorSetLayoutBinding>::with_size(key.resource_count, temp_allocator());
    for (int i = 0; i < bindings.size(); ++i)
    {
        bindings[i].binding = key.resources[i].binding;
        bindings[i].descriptorType = convert_resource_binding_type(key.resources[i].type);
        bindings[i].descriptorCount = key.resources[i].element_count;
        bindings[i].stageFlags = decode_shader_stage(key.resources[i].shader_stages);
        bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr };
    info.flags = 0;
    info.bindingCount = key.resource_count;
    info.pBindings = bindings.data();

    BEE_VK_CHECK(vkCreateDescriptorSetLayout(device->handle, &info, nullptr, layout));
}

void destroy_descriptor_set_layout(VulkanDevice* device, VkDescriptorSetLayout* layout)
{
    vkDestroyDescriptorSetLayout(device->handle, *layout, nullptr);
}

void create_pipeline_layout(VulkanDevice* device, const VulkanPipelineLayoutKey& key, VkPipelineLayout* layout)
{
    auto descriptor_set_layouts = FixedArray<VkDescriptorSetLayout>::with_size(key.resource_layout_count, temp_allocator());
    for (int i = 0; i < descriptor_set_layouts.size(); ++i)
    {
        descriptor_set_layouts[i] = device->descriptor_set_layout_cache.get_or_create(key.resource_layouts[i]);
    }

    auto push_constants = FixedArray<VkPushConstantRange>::with_size(key.push_constant_range_count, temp_allocator());
    for (int i = 0; i < push_constants.size(); ++i)
    {
        push_constants[i].stageFlags = decode_shader_stage(key.push_constant_ranges[i].shader_stages);
        push_constants[i].offset = key.push_constant_ranges[i].offset;
        push_constants[i].size = key.push_constant_ranges[i].size;
    }

    VkPipelineLayoutCreateInfo info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr };
    info.flags = 0;
    info.setLayoutCount = key.resource_layout_count;
    info.pSetLayouts = descriptor_set_layouts.data();
    info.pushConstantRangeCount = key.push_constant_range_count;
    info.pPushConstantRanges = push_constants.data();
    BEE_VK_CHECK(vkCreatePipelineLayout(device->handle, &info, nullptr, layout));
}

void destroy_pipeline_layout(VulkanDevice* device, VkPipelineLayout* layout)
{
    vkDestroyPipelineLayout(device->handle, *layout, nullptr);
}

void create_framebuffer(VulkanDevice* device, const VulkanFramebufferKey& key, VkFramebuffer* framebuffer)
{
    VkFramebufferCreateInfo info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr };
    info.flags = 0;
    info.renderPass = key.compatible_render_pass;
    info.attachmentCount = key.attachment_count;
    info.pAttachments = key.attachments;
    info.width = key.width;
    info.height = key.height;
    info.layers = key.layers;

    BEE_VK_CHECK(vkCreateFramebuffer(device->handle, &info, nullptr, framebuffer));
}

void destroy_framebuffer(VulkanDevice* device, VkFramebuffer* framebuffer)
{
    vkDestroyFramebuffer(device->handle, *framebuffer, nullptr);
}


} // namespace bee