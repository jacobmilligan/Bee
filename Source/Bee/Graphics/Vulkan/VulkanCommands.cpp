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


#define BEE_VK_CMD(cmd) cmd; ++size_

/*
 ********************************************************
 *
 * # GPU Command buffer API
 *
 ********************************************************
 */
CommandBuffer::CommandBuffer(const DeviceHandle& device_handle, const CommandPoolHandle& pool_handle, const QueueType required_queue_type)
{
    auto& device = validate_device(device_handle);
    auto& pool = device.command_pools[pool_handle];
    int queue = 0; // by default we allocate from the combined graphics/compute/transfer queue

    switch (required_queue_type)
    {
        case QueueType::graphics:
        case QueueType::compute:
        case QueueType::transfer:
        {
            queue = queue_type_index(required_queue_type);
            break;
        }
        case QueueType::none:
        {
            queue = -1;
            break;
        }
        default:
        {
            break;
        }
    }

    auto& queue_pool = pool.per_queue_pools[queue];

    BEE_ASSERT(queue >= 0);
    BEE_ASSERT_F(
        queue_pool.handle != VK_NULL_HANDLE,
        "Cannot create command buffer with queue type (%u): the command pool cannot allocate from that queue family",
        static_cast<u32>(required_queue_type)
    );

    VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
    alloc_info.commandPool = pool.per_queue_pools[queue].handle;
    alloc_info.commandBufferCount = 1u;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    native = BEE_NEW(pool.allocator, NativeCommandBuffer);
    native->index = queue_pool.command_buffers.size();
    native->queue = queue;
    native->pool = pool_handle;
    native->device = &device;
    native->api = this;
    BEE_VK_CHECK(vkAllocateCommandBuffers(device.handle, &alloc_info, &native->handle));

    queue_pool.command_buffers.push_back(native);
    state_ = CommandBufferState::initial;
}

CommandBuffer::~CommandBuffer()
{
    if (native == nullptr)
    {
        return;
    }

    auto& pool = native->device->command_pools[native->pool];
    auto& per_queue_pool = pool.per_queue_pools[native->queue];
    vkFreeCommandBuffers(native->device->handle, per_queue_pool.handle, 1, &native->handle);

    BEE_DELETE(pool.allocator, native);

    if (native->index != per_queue_pool.command_buffers.size() - 1)
    {
        std::swap(per_queue_pool.command_buffers.back(), per_queue_pool.command_buffers[native->index]);
        per_queue_pool.command_buffers[native->index]->index = native->index;
    }

    per_queue_pool.command_buffers.pop_back();
    state_ = CommandBufferState::invalid;
}

void CommandBuffer::reset(const CommandStreamReset hint)
{
    const auto reset_flags = convert_command_buffer_reset_hint(hint);

    // the command buffer doesn't target a swapchain anymore
    native->target_swapchain = SwapchainHandle{};

    BEE_VK_CHECK(vkResetCommandBuffer(native->handle, reset_flags));

    state_ = CommandBufferState::initial;
    size_ = 0;
}

void CommandBuffer::begin(const CommandBufferUsage usage)
{
    VkCommandBufferBeginInfo info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
    info.flags = convert_command_buffer_usage(usage);

    BEE_VK_CHECK(vkBeginCommandBuffer(native->handle, &info));

    state_ = CommandBufferState::recording;
}

void CommandBuffer::end()
{
    BEE_VK_CHECK(vkEndCommandBuffer(native->handle));

    state_ = size_ > 0 ? CommandBufferState::executable : CommandBufferState::empty;
}

void CommandBuffer::begin_render_pass(
    const RenderPassHandle&     pass_handle,
    const u32                   attachment_count,
    const TextureViewHandle*    attachments,
    const RenderRect&           render_area,
    const u32                   clear_value_count,
    const ClearValue*           clear_values
)
{
    auto& pass = native->device->render_passes[pass_handle];
    auto begin_info = VkRenderPassBeginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
    begin_info.renderPass = pass.handle;
    begin_info.renderArea = vkrect2d_cast(render_area);
    begin_info.clearValueCount = clear_value_count;

    VulkanFramebufferKey fb_key{};
    fb_key.width = render_area.width;
    fb_key.height = render_area.height;
    fb_key.layers = 1;
    fb_key.attachment_count = attachment_count;

    VkImageView image_views[BEE_GPU_MAX_ATTACHMENTS];

    for (u32 i = 0; i < attachment_count; ++i)
    {
        auto& view = native->device->texture_views[attachments[i]];

        image_views[i] = view.handle;

        fb_key.attachments[i].format = view.format;
        fb_key.attachments[i].sample_count = view.samples;
    }

    begin_info.framebuffer = get_or_create_framebuffer(native->device, fb_key, pass.handle, image_views);

    VkClearValue vk_clear_values[BEE_GPU_MAX_ATTACHMENTS];
    for (u32 val = 0; val < clear_value_count; ++val)
    {
        memcpy(vk_clear_values + val, clear_values + val, sizeof(VkClearValue));
    }

    auto& device_texture_views = native->device->texture_views;
    for (u32 att = 0; att < attachment_count; ++att)
    {
        auto& texture_view = attachments[att];
        if (device_texture_views[texture_view].swapchain.is_valid())
        {
            BEE_ASSERT_F(!native->target_swapchain.is_valid(), "A render pass must contain only one swapchain texture attachment");
            native->target_swapchain = device_texture_views[texture_view].swapchain;
        }
    }

    begin_info.pClearValues = vk_clear_values;

    // TODO(Jacob): if we switch to secondary command buffers, this flag should change
    BEE_VK_CMD(vkCmdBeginRenderPass(native->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE));
}

void CommandBuffer::end_render_pass()
{
    BEE_VK_CMD(vkCmdEndRenderPass(native->handle));
}

void CommandBuffer::bind_pipeline_state(const PipelineStateHandle& pipeline_handle)
{
    auto& pipeline = native->device->pipelines[pipeline_handle];
    BEE_VK_CMD(vkCmdBindPipeline(native->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle));
}

void CommandBuffer::bind_vertex_buffer(const BufferHandle& buffer_handle, const u32 binding, const u64 offset)
{
    bind_vertex_buffers(binding, 1, &buffer_handle, &offset);
}

void CommandBuffer::bind_vertex_buffers(
    const u32           first_binding,
    const u32           count,
    const BufferHandle* buffers,
    const u64*          offsets
)
{
    auto& buffer_table = native->device->buffers;
    auto* vk_buffers = BEE_ALLOCA_ARRAY(VkBuffer, count);

    for (u32 b = 0; b < count; ++b)
    {
        vk_buffers[b] = buffer_table[buffers[b]].handle;
    }

    BEE_VK_CMD(vkCmdBindVertexBuffers(native->handle, first_binding, count, vk_buffers, offsets));
}

void CommandBuffer::bind_index_buffer(const BufferHandle& buffer_handle, const u64 offset, const IndexFormat index_format)
{
    auto& buffer = native->device->buffers[buffer_handle];
    BEE_VK_CMD(vkCmdBindIndexBuffer(native->handle, buffer.handle, offset, convert_index_type(index_format)));
}

void CommandBuffer::copy_buffer(
    const BufferHandle& src_handle,
    const i32           src_offset,
    const BufferHandle& dst_handle,
    const i32           dst_offset,
    const i32           size
)
{
    auto& src = native->device->buffers[src_handle];
    auto& dst = native->device->buffers[dst_handle];

    VkBufferCopy copy{};
    copy.srcOffset = src_offset;
    copy.dstOffset = dst_offset;
    copy.size = size;

    BEE_VK_CMD(vkCmdCopyBuffer(native->handle, src.handle, dst.handle, 1, &copy));
}

void CommandBuffer::draw(const u32 vertex_count, const u32 instance_count, const u32 first_vertex, const u32 first_instance)
{
    BEE_VK_CMD(vkCmdDraw(native->handle, vertex_count, instance_count, first_vertex, first_instance));
}

void CommandBuffer::draw_indexed(
    const u32 index_count,
    const u32 instance_count,
    const u32 vertex_offset,
    const u32 first_index,
    const u32 first_instance
)
{
    BEE_VK_CMD(vkCmdDrawIndexed(
        native->handle,
        index_count,
        instance_count,
        first_index,
        vertex_offset,
        first_instance
    ));
}

void CommandBuffer::set_viewport(const Viewport& viewport)
{
    VkViewport vk_viewport{};
    vk_viewport.x = viewport.x;
    vk_viewport.y = viewport.y;
    vk_viewport.width = viewport.width;
    vk_viewport.height = viewport.height;
    vk_viewport.minDepth = viewport.min_depth;
    vk_viewport.maxDepth = viewport.max_depth;

    BEE_VK_CMD(vkCmdSetViewport(native->handle, 0, 1, &vk_viewport));
}

void CommandBuffer::set_scissor(const RenderRect& scissor)
{
    VkRect2D rect{};
    rect.offset.x = scissor.x_offset;
    rect.offset.y = scissor.y_offset;
    rect.extent.width = scissor.width;
    rect.extent.height = scissor.height;

    BEE_VK_CMD(vkCmdSetScissor(native->handle, 0, 1, &rect));
}

void CommandBuffer::transition_resources(const u32 count, const GpuTransition* transitions)
{
    DynamicArray<VkImageMemoryBarrier> image_barriers(temp_allocator());
    DynamicArray<VkBufferMemoryBarrier> buffer_barriers(temp_allocator());
    DynamicArray<VkMemoryBarrier> memory_barriers(temp_allocator());

    VkAccessFlags src_access = 0;
    VkAccessFlags dst_access = 0;

    for (u32 i = 0; i < count; ++i)
    {
        const auto& transition = transitions[i];

        switch (transition.barrier_type)
        {
            case GpuBarrierType::texture:
            {
                image_barriers.emplace_back();

                auto& barrier = image_barriers.back();
                auto& texture = native->device->textures[transition.barrier.texture];

                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = convert_access_mask(transition.old_state);
                barrier.dstAccessMask = convert_access_mask(transition.new_state);
                barrier.oldLayout = convert_image_layout(transition.old_state);
                barrier.newLayout = convert_image_layout(transition.new_state);
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = texture.handle;
                barrier.subresourceRange.aspectMask = select_access_mask_from_format(texture.format);
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = texture.levels;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = texture.layers;

                src_access |= barrier.srcAccessMask;
                dst_access |= barrier.srcAccessMask;
                break;
            }
            case GpuBarrierType::buffer:
            {
                buffer_barriers.emplace_back();

                auto& barrier = buffer_barriers.back();
                auto& buffer = native->device->buffers[transition.barrier.buffer.handle];

                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.pNext = nullptr;
                barrier.srcAccessMask = convert_access_mask(transition.old_state);
                barrier.dstAccessMask = convert_access_mask(transition.new_state);
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = buffer.handle;
                barrier.offset = transition.barrier.buffer.offset;
                barrier.size = transition.barrier.buffer.size;

                if (barrier.size == 0)
                {
                    barrier.size = buffer.size - barrier.offset;
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

    auto src_stage = select_pipeline_stage_from_access(src_access);
    auto dst_stage = select_pipeline_stage_from_access(dst_access);

    if (src_stage == 0)
    {
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    if (dst_stage == 0)
    {
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    BEE_VK_CMD(vkCmdPipelineBarrier(
        native->handle,
        src_stage, dst_stage,
        0,
        static_cast<u32>(memory_barriers.size()), memory_barriers.data(),
        static_cast<u32>(buffer_barriers.size()), buffer_barriers.data(),
        static_cast<u32>(image_barriers.size()), image_barriers.data()
    ));
}


} // namespace bee