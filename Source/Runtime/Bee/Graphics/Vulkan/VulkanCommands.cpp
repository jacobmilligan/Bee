/*
 *  VulkanCommands.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */


#include "Bee/Graphics/GPU.hpp"
#include "Bee/Graphics/Vulkan/VulkanBackend.hpp"
#include "Bee/Graphics/Vulkan/VulkanConvert.hpp"

#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


struct VulkanCommandContext
{
    VulkanDevice*       device { nullptr };
    VulkanCommandPool*  command_pool { nullptr };
    VkCommandBuffer     command_buffer { VK_NULL_HANDLE };
};

void gpu_record_command(GpuCommandBuffer* cmd, const CmdBeginRenderPass& data)
{
    auto pass = cmd->device->render_passes[data.pass];
    auto begin_info = VkRenderPassBeginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
    begin_info.renderPass = pass->handle;
    begin_info.renderArea = vkrect2d_cast(data.render_area);
    begin_info.clearValueCount = data.clear_value_count;

    VulkanFramebufferKey fb_key{};
    fb_key.width = data.render_area.width;
    fb_key.height = data.render_area.height;
    fb_key.layers = 1;
    fb_key.attachment_count = data.attachment_count;

    VkImageView image_views[BEE_GPU_MAX_ATTACHMENTS];

    for (u32 i = 0; i < data.attachment_count; ++i)
    {
        auto view = cmd->device->texture_views[data.attachments[i]];

        image_views[i] = view->handle;

        fb_key.attachments[i].format = view->format;
        fb_key.attachments[i].sample_count = view->samples;
    }

    begin_info.framebuffer = get_or_create_framebuffer(cmd->device, fb_key, pass->handle, image_views);

    VkClearValue clear_values[BEE_GPU_MAX_ATTACHMENTS];
    for (u32 val = 0; val < data.clear_value_count; ++val)
    {
        memcpy(clear_values + val, data.clear_values + val, sizeof(VkClearValue));
    }

    auto& device_texture_views = cmd->device->texture_views;
    for (u32 att = 0; att < data.attachment_count; ++att)
    {
        auto& texture_view = data.attachments[att];
        if (device_texture_views[texture_view]->swapchain_handle.is_valid())
        {
            BEE_ASSERT_F(!cmd->target_swapchain.is_valid(), "A render pass must contain only one swapchain texture attachment");
            cmd->target_swapchain = device_texture_views[texture_view]->swapchain_handle;
        }
    }

    begin_info.pClearValues = clear_values;

    // TODO(Jacob): if we switch to secondary command buffers, this flag should change
    vkCmdBeginRenderPass(cmd->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void gpu_record_command(GpuCommandBuffer* cmd, const CmdEndRenderPass& /* data */)
{
    vkCmdEndRenderPass(cmd->handle);
}

void gpu_record_command(GpuCommandBuffer* cmd, const CmdCopyBuffer& data)
{
    auto src = cmd->device->buffers[data.src];
    auto dst = cmd->device->buffers[data.src];

    VkBufferCopy copy{};
    copy.srcOffset = data.src_offset;
    copy.dstOffset = data.dst_offset;
    copy.size = data.size;

    vkCmdCopyBuffer(cmd->handle, src->handle, dst->handle, 1, &copy);
}

void gpu_record_command(GpuCommandBuffer* cmd, const CmdDraw& data)
{
    vkCmdDraw(cmd->handle, data.vertex_count, data.instance_count, data.first_vertex, data.first_instance);
}

void gpu_record_command(GpuCommandBuffer* cmd, const CmdDrawIndexed& data)
{
    vkCmdDrawIndexed(
        cmd->handle,
        data.index_count,
        data.instance_count,
        data.first_index,
        data.vertex_offset,
        data.first_instance
    );
}

void gpu_record_command(GpuCommandBuffer* cmd, const CmdTransitionResources& data)
{
    DynamicArray<VkImageMemoryBarrier> image_barriers(job_temp_allocator());
    DynamicArray<VkBufferMemoryBarrier> buffer_barriers(job_temp_allocator());
    DynamicArray<VkMemoryBarrier> memory_barriers(job_temp_allocator());

    VkAccessFlags src_access = 0;
    VkAccessFlags dst_access = 0;

    for (u32 i = 0; i < data.count; ++i)
    {
        auto& transition = data.transitions[i];

        switch (transition.barrier_type)
        {
            case GpuBarrierType::texture:
            {
                image_barriers.emplace_back();

                auto& barrier = image_barriers.back();
                auto texture = cmd->device->textures[transition.barrier.texture];

                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = convert_access_mask(transition.old_state);
                barrier.dstAccessMask = convert_access_mask(transition.new_state);
                barrier.oldLayout = convert_image_layout(transition.old_state);
                barrier.newLayout = convert_image_layout(transition.new_state);
                barrier.srcQueueFamilyIndex = 0;
                barrier.dstQueueFamilyIndex = 0;
                barrier.image = texture->handle;
                barrier.subresourceRange.aspectMask = select_access_mask_from_format(texture->format);
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = texture->levels;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = texture->layers;

                src_access |= barrier.srcAccessMask;
                dst_access |= barrier.srcAccessMask;
                break;
            }
            case GpuBarrierType::buffer:
            {
                buffer_barriers.emplace_back();

                auto& barrier = buffer_barriers.back();
                auto buffer = cmd->device->buffers[transition.barrier.buffer.handle];

                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = convert_access_mask(transition.old_state);
                barrier.dstAccessMask = convert_access_mask(transition.new_state);
                barrier.srcQueueFamilyIndex = 0;
                barrier.dstQueueFamilyIndex = 0;
                barrier.buffer = buffer->handle;
                barrier.offset = transition.barrier.buffer.offset;
                barrier.size = transition.barrier.buffer.size;

                if (barrier.size == 0)
                {
                    barrier.size = buffer->size - barrier.offset;
                }

                src_access |= barrier.srcAccessMask;
                dst_access |= barrier.srcAccessMask;
                break;
            }
            case GpuBarrierType::memory:
            {
                memory_barriers.emplace_back();

                auto& barrier = memory_barriers.back();
                barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = convert_access_mask(transition.old_state);
                barrier.dstAccessMask = convert_access_mask(transition.new_state);

                src_access |= barrier.srcAccessMask;
                dst_access |= barrier.srcAccessMask;
                break;
            }
            default:
            {
                BEE_UNREACHABLE("Invalid barrier type");
            }
        }
    }

    const auto src_stage = select_pipeline_stage_from_access(src_access);
    const auto dst_stage = select_pipeline_stage_from_access(dst_access);

    vkCmdPipelineBarrier(
        cmd->handle,
        src_stage, dst_stage,
        0,
        static_cast<u32>(memory_barriers.size()), memory_barriers.data(),
        static_cast<u32>(buffer_barriers.size()), buffer_barriers.data(),
        static_cast<u32>(image_barriers.size()), image_barriers.data()
    );
}


} // namespace bee