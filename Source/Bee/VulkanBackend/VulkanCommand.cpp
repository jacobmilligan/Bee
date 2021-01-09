/*
 *  VulkanCommand.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/VulkanBackend/VulkanDevice.hpp"
#include "Bee/VulkanBackend/VulkanConvert.hpp"


namespace bee {


void begin(CommandBuffer* cmd_buf, const CommandBufferUsage usage)
{
    VkCommandBufferBeginInfo info { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
    info.flags = convert_command_buffer_usage(usage);
    BEE_VK_CHECK(vkBeginCommandBuffer(cmd_buf->handle, &info));

    cmd_buf->state = CommandBufferState::recording;
}

void end(CommandBuffer* cmd_buf)
{
    BEE_VK_CHECK(vkEndCommandBuffer(cmd_buf->handle));

    cmd_buf->state = CommandBufferState::executable;
}

void reset(CommandBuffer* cmd_buf, const CommandStreamReset hint)
{
    const auto flags = convert_command_buffer_reset_hint(hint);
    BEE_VK_CHECK(vkResetCommandBuffer(cmd_buf->handle, flags));

    cmd_buf->reset(cmd_buf->device);
}

CommandBufferState get_state(CommandBuffer* cmd_buf)
{
    return cmd_buf->state;
}

void allocate_dynamic_binding(VulkanDevice* device, VulkanResourceBinding* binding)
{
    auto* pool = get_or_create_descriptor_pool(device, binding->update_frequency, binding->layout);
    VkDescriptorSetAllocateInfo set_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr };
    set_info.descriptorPool = binding->pool->handle;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &pool->layout;

    BEE_VK_CHECK(vkAllocateDescriptorSets(device->handle, &set_info, &binding->set));

    binding->allocated_frame = device->current_frame;
}

bool setup_draw(CommandBuffer* cmd_buf)
{
    if (cmd_buf->bound_pipeline == nullptr)
    {
        log_error("Cannot execute draw command without first binding a PipelineState");
        return false;
    }

    for (int i = 0; i < static_array_length(cmd_buf->descriptors); ++i)
    {
        if (cmd_buf->descriptors[i] != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(
                cmd_buf->handle,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                cmd_buf->bound_pipeline->layout,
                i,
                1,
                cmd_buf->descriptors + i,
                0,
                nullptr
            );
        }
    }

    return true;
}

/*
 ********************
 *
 * Render commands
 *
 ********************
 */
void begin_render_pass(
    CommandBuffer*              cmd_buf,
    const RenderPassHandle&     pass_handle,
    const u32                   attachment_count,
    const TextureViewHandle*    attachments,
    const RenderRect&           render_area,
    const u32                   clear_value_count,
    const ClearValue*           clear_values
)
{
    if (BEE_FAIL_F(cmd_buf->current_render_pass == nullptr, "Cannot begin a new render pass while inside another render pass"))
    {
        return;
    }

    auto& pass = cmd_buf->device->render_passes_get(pass_handle);
    cmd_buf->current_render_pass = &pass;

    auto begin_info = VkRenderPassBeginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr };
    begin_info.renderPass = pass.handle;
    begin_info.renderArea = vkrect2d_cast(render_area);
    begin_info.clearValueCount = clear_value_count;

    VulkanFramebufferKey fb_key{};
    fb_key.width = render_area.width;
    fb_key.height = render_area.height;
    fb_key.layers = 1;
    fb_key.attachment_count = attachment_count;
    fb_key.compatible_render_pass = pass.handle;

    for (u32 i = 0; i < attachment_count; ++i)
    {
        auto& view = cmd_buf->device->texture_views_get(attachments[i]);
        fb_key.attachments[i] = view.handle;
        fb_key.format_keys[i].format = view.format;
        fb_key.format_keys[i].sample_count = view.samples;
    }

    begin_info.framebuffer = cmd_buf->device->framebuffer_cache.get_or_create(fb_key);

    VkClearValue vk_clear_values[BEE_GPU_MAX_ATTACHMENTS];
    for (u32 val = 0; val < clear_value_count; ++val)
    {
        memcpy(vk_clear_values + val, clear_values + val, sizeof(VkClearValue));
    }

    for (u32 att = 0; att < attachment_count; ++att)
    {
        auto& view = cmd_buf->device->texture_views_get(attachments[att]);
        if (view.swapchain >= 0)
        {
            BEE_ASSERT_F(cmd_buf->target_swapchain < 0, "A render pass must contain only one swapchain texture attachment");
            cmd_buf->target_swapchain = view.swapchain;
        }
    }

    begin_info.pClearValues = vk_clear_values;

    // TODO(Jacob): if we switch to secondary command buffers, this flag should change
    vkCmdBeginRenderPass(cmd_buf->handle, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void end_render_pass(CommandBuffer* cmd_buf)
{
    cmd_buf->current_render_pass = nullptr;
    vkCmdEndRenderPass(cmd_buf->handle);
}

void bind_vertex_buffers(
    CommandBuffer*      cmd_buf,
    const u32           first_binding,
    const u32           count,
    const BufferHandle* buffers,
    const u64*          offsets
)
{
    auto* device = cmd_buf->device;
    auto* vk_buffers = BEE_ALLOCA_ARRAY(VkBuffer, count);

    for (u32 b = 0; b < count; ++b)
    {
        vk_buffers[b] = device->buffers_get(buffers[b]).handle;
    }

    vkCmdBindVertexBuffers(cmd_buf->handle, first_binding, count, vk_buffers, offsets);
}

void bind_vertex_buffer(CommandBuffer* cmd_buf, const BufferHandle& buffer_handle, const u32 binding, const u64 offset)
{
    bind_vertex_buffers(cmd_buf, binding, 1, &buffer_handle, &offset);
}

void bind_index_buffer(CommandBuffer* cmd_buf, const BufferHandle& buffer_handle, const u64 offset, const IndexFormat index_format)
{
    auto& buffer = cmd_buf->device->buffers_get(buffer_handle);
    vkCmdBindIndexBuffer(cmd_buf->handle, buffer.handle, offset, convert_index_type(index_format));
}

void copy_buffer(
    CommandBuffer*      cmd_buf,
    const BufferHandle& src_handle,
    const i32           src_offset,
    const BufferHandle& dst_handle,
    const i32           dst_offset,
    const i32           size
)
{
    auto& src = cmd_buf->device->buffers_get(src_handle);
    auto& dst = cmd_buf->device->buffers_get(dst_handle);

    VkBufferCopy copy{};
    copy.srcOffset = src_offset;
    copy.dstOffset = dst_offset;
    copy.size = size;

    vkCmdCopyBuffer(cmd_buf->handle, src.handle, dst.handle, 1, &copy);
}

void draw(
    CommandBuffer*                  cmd_buf,
    const PipelineStateDescriptor&  state,
    const u32                       vertex_count,
    const u32                       instance_count,
    const u32                       first_vertex,
    const u32                       first_instance
)
{
    VulkanPipelineKey key{};
    key.desc = &state;
    key.render_pass_hash = cmd_buf->current_render_pass->hash;
    key.render_pass = cmd_buf->current_render_pass->handle;
    key.shader_hashes[ShaderStageIndex::vertex] = cmd_buf->device->shaders_get(state.vertex_stage).hash;
    key.shader_hashes[ShaderStageIndex::fragment] = cmd_buf->device->shaders_get(state.fragment_stage).hash;
    auto& pipeline = cmd_buf->device->pipeline_cache.get_or_create(key);

    vkCmdBindPipeline(cmd_buf->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmd_buf->handle, vertex_count, instance_count, first_vertex, first_instance);
}

void draw_indexed(
    CommandBuffer*                  cmd_buf,
    const PipelineStateDescriptor&  state,
    const u32                       index_count,
    const u32                       instance_count,
    const u32                       vertex_offset,
    const u32                       first_index,
    const u32                       first_instance
)
{
    VulkanPipelineKey key{};
    key.desc = &state;
    key.render_pass_hash = cmd_buf->current_render_pass->hash;
    key.render_pass = cmd_buf->current_render_pass->handle;
    key.shader_hashes[ShaderStageIndex::vertex] = cmd_buf->device->shaders_get(state.vertex_stage).hash;
    key.shader_hashes[ShaderStageIndex::fragment] = cmd_buf->device->shaders_get(state.fragment_stage).hash;
    auto& pipeline = cmd_buf->device->pipeline_cache.get_or_create(key);

    vkCmdBindPipeline(cmd_buf->handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDrawIndexed(
        cmd_buf->handle,
        index_count,
        instance_count,
        first_index,
        vertex_offset,
        first_instance
    );
}

void set_viewport(CommandBuffer* cmd_buf, const Viewport& viewport)
{
    VkViewport vk_viewport{};
    vk_viewport.x = viewport.x;
    vk_viewport.y = viewport.y;
    vk_viewport.width = viewport.width;
    vk_viewport.height = viewport.height;
    vk_viewport.minDepth = viewport.min_depth;
    vk_viewport.maxDepth = viewport.max_depth;

    vkCmdSetViewport(cmd_buf->handle, 0, 1, &vk_viewport);
}

void set_scissor(CommandBuffer* cmd_buf, const RenderRect& scissor)
{
    VkRect2D rect{};
    rect.offset.x = scissor.x_offset;
    rect.offset.y = scissor.y_offset;
    rect.extent.width = scissor.width;
    rect.extent.height = scissor.height;

    vkCmdSetScissor(cmd_buf->handle, 0, 1, &rect);
}

void transition_resources(CommandBuffer* cmd_buf, const u32 count, const GpuTransition* transitions)
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
                auto& texture = cmd_buf->device->textures_get(transition.barrier.texture);

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
                dst_access |= barrier.dstAccessMask;
                break;
            }
            case GpuBarrierType::buffer:
            {
                buffer_barriers.emplace_back();

                auto& barrier = buffer_barriers.back();
                auto& buffer = cmd_buf->device->buffers_get(transition.barrier.buffer.handle);

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
                dst_access |= barrier.dstAccessMask;
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
                dst_access |= barrier.dstAccessMask;
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

    vkCmdPipelineBarrier(
        cmd_buf->handle,
        src_stage, dst_stage,
        0,
        static_cast<u32>(memory_barriers.size()), memory_barriers.data(),
        static_cast<u32>(buffer_barriers.size()), buffer_barriers.data(),
        static_cast<u32>(image_barriers.size()), image_barriers.data()
    );
}

void bind_resources(CommandBuffer* cmd_buf, const u32 layout_index, const ResourceBindingHandle& resource_binding)
{
    if (layout_index >= BEE_GPU_MAX_RESOURCE_LAYOUTS)
    {
        log_error("Cannot bind more than BEE_GPU_MAX_RESOURCE_LAYOUTS (%d) resource binding handles per draw", BEE_GPU_MAX_RESOURCE_LAYOUTS);
        return;
    }

    auto& binding = cmd_buf->device->resource_bindings_get(resource_binding);

    if (binding.set == VK_NULL_HANDLE && binding.update_frequency != ResourceBindingUpdateFrequency::persistent)
    {
        allocate_dynamic_binding(cmd_buf->device, &binding);
    }

    cmd_buf->descriptors[layout_index] = binding.set;
}


} // namespace bee

void bee_load_command_backend(bee::GpuCommandBackend* api)
{
    // Control commands
    api->reset = bee::reset;
    api->begin = bee::begin;
    api->end = bee::end;
    api->get_state = bee::get_state;

    // Render commands
    api->begin_render_pass = bee::begin_render_pass;
    api->end_render_pass = bee::end_render_pass;
    api->bind_vertex_buffer = bee::bind_vertex_buffer;
    api->bind_vertex_buffers = bee::bind_vertex_buffers;
    api->bind_index_buffer = bee::bind_index_buffer;
    api->copy_buffer = bee::copy_buffer;
    api->draw = bee::draw;
    api->draw_indexed = bee::draw_indexed;
    api->set_viewport = bee::set_viewport;
    api->set_scissor = bee::set_scissor;
    api->transition_resources = bee::transition_resources;
    api->bind_resources = bee::bind_resources;
}
