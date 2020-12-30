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
    auto bindings = FixedArray<VkDescriptorSetLayoutBinding>::with_size(key.resources.size, temp_allocator());
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
    info.bindingCount = key.resources.size;
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

void create_pipeline(VulkanDevice* device, const VulkanPipelineKey& key, VkPipeline* pipeline)
{
    auto* desc = key.desc;

    /*
     * Shader stages
     */
    struct StageInfo
    {
        ShaderHandle handle;
        VkShaderStageFlagBits flags { VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM };
    };

    StageInfo shaders[] = {
        { desc->vertex_stage, VK_SHADER_STAGE_VERTEX_BIT },
        { desc->fragment_stage, VK_SHADER_STAGE_FRAGMENT_BIT }
    };

    DynamicArray<VkPipelineShaderStageCreateInfo> stages(temp_allocator());

    for (const auto& stage : shaders)
    {
        if (!stage.handle.is_valid())
        {
            continue;
        }

        auto& thread = device->get_thread(stage.handle);
        auto& shader = thread.shaders[stage.handle];

        stages.emplace_back();

        auto& stage_info = stages.back();
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.pNext = nullptr;
        stage_info.flags = 0;
        stage_info.stage = stage.flags;
        stage_info.module = shader.handle;
        stage_info.pName = shader.entry.c_str();
        stage_info.pSpecializationInfo = nullptr;
    }

    /*
     * Vertex input state
     */
    auto vertex_binding_descs = FixedArray<VkVertexInputBindingDescription>::with_size(
        desc->vertex_description.layouts.size,
        temp_allocator()
    );
    auto vertex_attribute_descs = FixedArray<VkVertexInputAttributeDescription>::with_size(
        desc->vertex_description.attributes.size,
        temp_allocator()
    );

    for (int b = 0; b < vertex_binding_descs.size(); ++b)
    {
        auto& vk_desc = vertex_binding_descs[b];
        const auto& layout = desc->vertex_description.layouts[b];

        vk_desc.binding = layout.index;
        vk_desc.inputRate = convert_step_function(layout.step_function);
        vk_desc.stride = layout.stride;
    }

    for (int a = 0; a < vertex_attribute_descs.size(); ++a)
    {
        auto& vk_desc = vertex_attribute_descs[a];
        const auto& attr = desc->vertex_description.attributes[a];

        vk_desc.location = attr.location;
        vk_desc.binding = attr.layout;
        vk_desc.format = convert_vertex_format(attr.format);
        vk_desc.offset = attr.offset;
    }

    VkPipelineVertexInputStateCreateInfo vertex_info { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr };
    vertex_info.flags = 0;
    vertex_info.vertexBindingDescriptionCount = static_cast<u32>(vertex_binding_descs.size());
    vertex_info.pVertexBindingDescriptions = vertex_binding_descs.data();
    vertex_info.vertexAttributeDescriptionCount = static_cast<u32>(vertex_attribute_descs.size());
    vertex_info.pVertexAttributeDescriptions = vertex_attribute_descs.data();

    /*
     * Input assembly state
     */
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr };
    input_assembly_info.flags = 0;
    input_assembly_info.topology = convert_primitive_type(desc->primitive_type);
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    /*
     * TODO(Jacob): Tessellation state
     */

    /*
     * Viewport state
     */
    // setup a default viewport state - is required by Vulkan but it's values aren't used if the pipeline uses dynamic states
    VkPipelineViewportStateCreateInfo default_viewport_info { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr };
    default_viewport_info.flags = 0;
    default_viewport_info.viewportCount = 1;
    default_viewport_info.scissorCount = 1;

    /*
     * Rasterization state
     */
    VkPipelineRasterizationStateCreateInfo raster_info { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr };
    raster_info.flags = 0;
    raster_info.depthClampEnable = static_cast<VkBool32>(desc->raster_state.depth_clamp_enabled);
    raster_info.rasterizerDiscardEnable = VK_FALSE;
    raster_info.polygonMode = convert_fill_mode(desc->raster_state.fill_mode);
    raster_info.cullMode = convert_cull_mode(desc->raster_state.cull_mode);
    raster_info.frontFace = desc->raster_state.front_face_ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    raster_info.depthBiasEnable = static_cast<VkBool32>(desc->raster_state.depth_bias_enabled);
    raster_info.depthBiasConstantFactor = desc->raster_state.depth_bias;
    raster_info.depthBiasClamp = desc->raster_state.depth_bias_clamp;
    raster_info.depthBiasSlopeFactor = desc->raster_state.depth_slope_factor;
    raster_info.lineWidth = desc->raster_state.line_width;

    /*
     * Multisample state
     */
    VkPipelineMultisampleStateCreateInfo multisample_info { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr };
    multisample_info.flags = 0;
    multisample_info.rasterizationSamples = static_cast<VkSampleCountFlagBits>(desc->multisample_state.sample_count);
    multisample_info.sampleShadingEnable = static_cast<VkBool32>(desc->multisample_state.sample_shading_enabled);
    multisample_info.minSampleShading = desc->multisample_state.sample_shading;
    multisample_info.pSampleMask = &desc->multisample_state.sample_mask;
    multisample_info.alphaToCoverageEnable = static_cast<VkBool32>(desc->multisample_state.alpha_to_coverage_enabled);
    multisample_info.alphaToOneEnable = static_cast<VkBool32>(desc->multisample_state.alpha_to_one_enabled);

    /*
     * Depth-stencil state
     */
    static auto convert_stencil_op_descriptor = [](const StencilOpDescriptor& from, VkStencilOpState* to)
    {
        to->failOp = convert_stencil_op(from.fail_op);
        to->passOp = convert_stencil_op(from.pass_op);
        to->depthFailOp = convert_stencil_op(from.depth_fail_op);
        to->compareOp = convert_compare_func(from.compare_func);
        to->compareMask = from.read_mask;
        to->writeMask = from.write_mask;
        to->reference = from.reference;
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr };
    depth_stencil_info.flags = 0;
    depth_stencil_info.depthTestEnable = static_cast<VkBool32>(desc->depth_stencil_state.depth_test_enabled);
    depth_stencil_info.depthWriteEnable = static_cast<VkBool32>(desc->depth_stencil_state.depth_write_enabled);
    depth_stencil_info.depthCompareOp = convert_compare_func(desc->depth_stencil_state.depth_compare_func);
    depth_stencil_info.depthBoundsTestEnable = static_cast<VkBool32>(desc->depth_stencil_state.depth_bounds_test_enabled);
    depth_stencil_info.stencilTestEnable = static_cast<VkBool32>(desc->depth_stencil_state.stencil_test_enabled);
    convert_stencil_op_descriptor(desc->depth_stencil_state.front_face_stencil, &depth_stencil_info.front);
    convert_stencil_op_descriptor(desc->depth_stencil_state.back_face_stencil, &depth_stencil_info.front);
    depth_stencil_info.minDepthBounds = desc->depth_stencil_state.min_depth_bounds;
    depth_stencil_info.maxDepthBounds = desc->depth_stencil_state.max_depth_bounds;

    /*
     * Color blend state
     */
    auto color_blend_attachments = FixedArray<VkPipelineColorBlendAttachmentState>::with_size(
        desc->color_blend_states.size,
        temp_allocator()
    );

    for (int i = 0; i < color_blend_attachments.size(); ++i)
    {
        auto& vk_state = color_blend_attachments[i];
        const auto& state = desc->color_blend_states[i];

        vk_state.blendEnable = static_cast<VkBool32>(state.blend_enabled);
        vk_state.srcColorBlendFactor = convert_blend_factor(state.src_blend_color);
        vk_state.dstColorBlendFactor = convert_blend_factor(state.dst_blend_color);
        vk_state.colorBlendOp = convert_blend_op(state.color_blend_op);
        vk_state.srcAlphaBlendFactor = convert_blend_factor(state.src_blend_alpha);
        vk_state.dstAlphaBlendFactor = convert_blend_factor(state.dst_blend_alpha);
        vk_state.alphaBlendOp = convert_blend_op(state.alpha_blend_op);
        vk_state.colorWriteMask = decode_color_write_mask(state.color_write_mask);
    }

    VkPipelineColorBlendStateCreateInfo color_blend_info { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr };
    color_blend_info.flags = 0;
    color_blend_info.logicOpEnable = VK_FALSE;
    color_blend_info.logicOp = VK_LOGIC_OP_CLEAR;
    color_blend_info.attachmentCount = desc->color_blend_states.size;
    color_blend_info.pAttachments = color_blend_attachments.data();
    color_blend_info.blendConstants[0] = 0.0f; // r
    color_blend_info.blendConstants[1] = 0.0f; // g
    color_blend_info.blendConstants[2] = 0.0f; // b
    color_blend_info.blendConstants[3] = 0.0f; // a

    /*
     * Dynamic state
     */
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr };
    dynamic_state_info.flags = 0;
    dynamic_state_info.dynamicStateCount = static_array_length(dynamic_states);
    dynamic_state_info.pDynamicStates = dynamic_states;

    /*
     * Pipeline layout
     */
    VulkanPipelineLayoutKey pipeline_layout_key{};
    pipeline_layout_key.resource_layout_count = desc->resource_layouts.size;
    pipeline_layout_key.resource_layouts = desc->resource_layouts.data;
    pipeline_layout_key.push_constant_range_count = desc->push_constant_ranges.size;
    pipeline_layout_key.push_constant_ranges = desc->push_constant_ranges.data;
    auto& pipeline_layout = device->pipeline_layout_cache.get_or_create(pipeline_layout_key);

    /*
     * TODO(Jacob): Pipeline cache
     */


    /*
     * Setup the pipeline state info
     */
    VkGraphicsPipelineCreateInfo info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr };
    info.flags = 0;
    info.stageCount = static_cast<u32>(stages.size());
    info.pStages = stages.data();
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &input_assembly_info;
    info.pTessellationState = nullptr;
    info.pViewportState = &default_viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &multisample_info;
    info.pDepthStencilState = &depth_stencil_info;
    info.pColorBlendState = &color_blend_info;
    info.pDynamicState = &dynamic_state_info;
    info.layout = pipeline_layout;
    info.renderPass = key.render_pass;
    info.subpass = key.subpass_index;
    info.basePipelineHandle = VK_NULL_HANDLE;
    info.basePipelineIndex = -1;

    // phew, that was a lot of typing - I think we earned ourselves a nice graphics pipeline object
    BEE_VK_CHECK(vkCreateGraphicsPipelines(device->handle, VK_NULL_HANDLE, 1, &info, nullptr, pipeline));
}

void destroy_pipeline(VulkanDevice* device, VkPipeline* pipeline)
{
    vkDestroyPipeline(device->handle, *pipeline, nullptr);
}


} // namespace bee