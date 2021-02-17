/*
 *  VulkanStaging.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Gpu/Gpu.hpp"
#include "Bee/VulkanBackend/VulkanDevice.hpp"

namespace bee {


void VulkanStaging::StagingBuffer::begin_commands()
{
    if (BEE_FAIL(cmd_state == CommandBufferState::initial))
    {
        return;
    }

    // begin the next buffers command recording
    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    for (auto& per_queue : cmd)
    {
        BEE_VK_CHECK(vkBeginCommandBuffer(per_queue, &begin_info));
    }

    offset = 0;
    cmd_state = CommandBufferState::recording;
}

void VulkanStaging::StagingBuffer::end_commands()
{
    if (BEE_FAIL(cmd_state == CommandBufferState::recording))
    {
        return;
    }

    for (auto& per_queue : cmd)
    {
        BEE_VK_CHECK(vkEndCommandBuffer(per_queue));
    }

    cmd_state = CommandBufferState::executable;
}

void VulkanStaging::StagingBuffer::submit_commands(VulkanDevice* device_ptr, VulkanQueue** queue_ptrs)
{
    static constexpr VkPipelineStageFlags graphics_wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (BEE_FAIL(cmd_state == CommandBufferState::executable))
    {
        return;
    }

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd[transfer_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &semaphores[transfer_index];
    queue_ptrs[transfer_index]->submit(submit_info, submit_fence[transfer_index], device_ptr);

    if (queue_ptrs[transfer_index] != queue_ptrs[graphics_index])
    {
        submit_info.pCommandBuffers = &cmd[graphics_index];
        submit_info.signalSemaphoreCount = 0;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &semaphores[transfer_index];
        submit_info.pWaitDstStageMask = &graphics_wait_stage;
//    submit_info.pSignalSemaphores = &semaphores[graphics_index];
        queue_ptrs[graphics_index]->submit(submit_info, submit_fence[graphics_index], device_ptr);
    }

    cmd_state = CommandBufferState::pending;
}

void VulkanStaging::StagingBuffer::wait_commands(VulkanDevice* device_ptr)
{
    // it's not an error to call wait_commands on a buffer that wasn't submitted previously - it's just a no-op instead
    if (cmd_state != CommandBufferState::pending)
    {
        return;
    }

    // Wait on the next buffer
    BEE_VK_CHECK(vkWaitForFences(device_ptr->handle, static_array_length(submit_fence), submit_fence, VK_TRUE, limits::max<u64>()));
    BEE_VK_CHECK(vkResetFences(device_ptr->handle, static_array_length(submit_fence), submit_fence));
    cmd_state = CommandBufferState::initial;
}

/*
 ******************************************
 *
 * Vulkan staging
 *
 ******************************************
 */
void VulkanStaging::submit()
{
    auto& buffer = buffers[current_buffer_index];

    if (buffer.allocation == VK_NULL_HANDLE)
    {
        return;
    }

    // avoid double submits
    if (buffer.cmd_state == CommandBufferState::recording)
    {
        buffer.end_commands();
        vmaFlushAllocation(vma_allocator, buffer.allocation, 0, buffer.offset);
        buffer.submit_commands(device, queues);
    }

    // advance to the next buffer
    current_buffer_index = (current_buffer_index + 1) % BEE_GPU_MAX_FRAMES_IN_FLIGHT;

    // if the next buffer wasn't submitted then we can skip waiting on it
    auto& next_buffer = buffers[current_buffer_index];
    next_buffer.wait_commands(device);
    next_buffer.begin_commands();
}

void VulkanStaging::init(VulkanDevice* new_device, VmaAllocator new_vma_allocator)
{
    BEE_ASSERT(device == VK_NULL_HANDLE);
    BEE_ASSERT(command_pool[0] == VK_NULL_HANDLE);
    BEE_ASSERT(command_pool[1] == VK_NULL_HANDLE);

    device = new_device;
    queues[transfer_index] = &new_device->transfer_queue;
    queues[graphics_index] = &new_device->graphics_queue;
    vma_allocator = new_vma_allocator;

    // create command pool before allocating staging buffers
    for (int queue_index = 0; queue_index < static_array_length(queues); ++queue_index)
    {
        VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queues[queue_index]->index;
        BEE_VK_CHECK(vkCreateCommandPool(device->handle, &pool_info, nullptr, &command_pool[queue_index]));

        VkCommandBuffer cmd_buffers[BEE_GPU_MAX_FRAMES_IN_FLIGHT];

        VkCommandBufferAllocateInfo cmd_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
        cmd_info.commandPool = command_pool[queue_index];
        cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_info.commandBufferCount = BEE_GPU_MAX_FRAMES_IN_FLIGHT;

        BEE_VK_CHECK(vkAllocateCommandBuffers(device->handle, &cmd_info, cmd_buffers));

        VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
        fence_info.flags = 0; // unsignalled

        VkSemaphoreCreateInfo sem_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr };
        sem_info.flags = 0;

        for (int buffer_index = 0; buffer_index < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++buffer_index)
        {
            auto& buffer = buffers[buffer_index];

            if (queue_index == 0)
            {
                new (&buffer) StagingBuffer{};
            }

            buffer.cmd[queue_index] = cmd_buffers[buffer_index];
            buffer.cmd_state = CommandBufferState::initial;

            BEE_VK_CHECK(vkCreateFence(device->handle, &fence_info, nullptr, &buffer.submit_fence[queue_index]));
            BEE_VK_CHECK(vkCreateSemaphore(device->handle, &sem_info, nullptr, &buffer.semaphores[queue_index]));
        }
    }

    buffers[0].begin_commands();
}

void VulkanStaging::destroy()
{
    BEE_ASSERT(device != VK_NULL_HANDLE);

    for (auto& buffer : buffers)
    {
        if (buffer.cmd_state == CommandBufferState::pending)
        {
            buffer.wait_commands(device);
        }

        if (buffer.allocation != nullptr)
        {
            vmaUnmapMemory(vma_allocator, buffer.allocation);
            vmaDestroyBuffer(vma_allocator, buffer.handle, buffer.allocation);
        }
        buffer.handle = VK_NULL_HANDLE;

        for (int i = 0; i < static_array_length(buffer.submit_fence); ++i)
        {
            vkDestroyFence(device->handle, buffer.submit_fence[i], nullptr);
            vkDestroySemaphore(device->handle, buffer.semaphores[i], nullptr);
        }
    }

    for (auto& pool : command_pool)
    {
        BEE_VK_CHECK(vkResetCommandPool(device->handle, pool, 0));
        vkDestroyCommandPool(device->handle, pool, nullptr);
    }
}

void VulkanStaging::ensure_capacity(const size_t capacity)
{
    if (capacity <= buffer_capacity)
    {
        return;
    }

    for (int i = 0; i < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        submit();
    }

    // Reallocate all staging buffers with new capacity
    VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr };
    info.flags = 0;
    info.size = capacity;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = nullptr;

    VmaAllocationCreateInfo vma_info{};
    vma_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    vma_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    vma_info.preferredFlags = 0;
    vma_info.memoryTypeBits = 0;
    vma_info.pool = VK_NULL_HANDLE;
    vma_info.pUserData = nullptr;

    for (auto& buffer : buffers)
    {
        // unmap and destroy current buffer
        if (buffer.handle != VK_NULL_HANDLE)
        {
            vmaUnmapMemory(vma_allocator, buffer.allocation);
            vmaDestroyBuffer(vma_allocator, buffer.handle, buffer.allocation);
            buffer.handle = VK_NULL_HANDLE;
        }

        BEE_VK_CHECK(vmaCreateBuffer(vma_allocator, &info, &vma_info, &buffer.handle, &buffer.allocation, &buffer.allocation_info));
        BEE_VK_CHECK(vmaMapMemory(vma_allocator, buffer.allocation, &buffer.data));
        set_vk_object_name(device, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, buffer.handle, "Staging Buffer");
    }

    buffer_capacity = capacity;
}

void VulkanStaging::allocate(const size_t size, const size_t alignment, VulkanStagingChunk* chunk)
{
    if (size > buffer_capacity)
    {
        ensure_capacity(size);
    }

    auto* buffer = &buffers[current_buffer_index];
    chunk->offset = round_up(buffer->offset, alignment);

    // flip to the next staging buffer if this chunk is about to exceed its capacity
    if (chunk->offset + size >= buffer_capacity && buffer->cmd_state != CommandBufferState::pending)
    {
        submit();
        buffer = &buffers[current_buffer_index];
        chunk->offset = buffer->offset;
    }

    // assign all the out parameters to the chunk
    chunk->data = reinterpret_cast<u8*>(buffer->data) + chunk->offset;
    chunk->buffer = buffer->handle;
    memcpy(chunk->cmd, buffer->cmd, static_array_length(buffer->cmd) * sizeof(VkCommandBuffer));

    // increment the buffers offset
    buffer->offset = chunk->offset + size;
    
    // begin command recording if not already recording
    if (buffer->cmd_state != CommandBufferState::recording)
    {
        buffer->begin_commands();
    }
}

bool VulkanStaging::is_pending()
{
    return buffers[current_buffer_index].offset > 0;
}


} // namespace bee
