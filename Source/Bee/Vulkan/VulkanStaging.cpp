/*
 *  VulkanStaging.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Plugins/Gpu/Gpu.hpp"
#include "Bee/Plugins/VulkanBackend/VulkanDevice.hpp"

namespace bee {


/*
 ******************************************
 *
 * Vulkan staging
 *
 ******************************************
 */
i32 VulkanStaging::submit_frame(const i32 frame)
{
    auto& buffer = buffers[frame];

    if (buffer.allocation == VK_NULL_HANDLE)
    {
        return frame;
    }

    BEE_VK_CHECK(vkEndCommandBuffer(buffer.cmd));
    vmaFlushAllocation(vma_allocator, buffer.allocation, 0, buffer.offset);

    VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &buffer.cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &buffer.transfer_semaphore;

    transfer_queue->submit(submit_info, buffer.submit_fence, device);

    buffer.is_submitted = true;
    return (frame + 1) % BEE_GPU_MAX_FRAMES_IN_FLIGHT;
}

void VulkanStaging::wait_on_frame(const i32 frame)
{
    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    auto& buffer = buffers[frame];
    if (!buffer.is_submitted)
    {
        return;
    }

    BEE_VK_CHECK(vkWaitForFences(device->handle, 1, &buffer.submit_fence, VK_TRUE, limits::max<u64>()));
    BEE_VK_CHECK(vkResetFences(device->handle, 1, &buffer.submit_fence));
    BEE_VK_CHECK(vkBeginCommandBuffer(buffer.cmd, &begin_info));

    buffer.is_submitted = false;
    buffer.offset = 0;
}

void VulkanStaging::init(VulkanDevice* new_device, VulkanQueue* new_transfer_queue, VmaAllocator new_vma_allocator)
{
    BEE_ASSERT(device == VK_NULL_HANDLE);
    BEE_ASSERT(command_pool == VK_NULL_HANDLE);

    device = new_device;
    transfer_queue = new_transfer_queue;
    vma_allocator = new_vma_allocator;

    // create command pool before allocating per frame staging buffers
    VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr };
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = transfer_queue->index;
    BEE_VK_CHECK(vkCreateCommandPool(device->handle, &pool_info, nullptr, &command_pool));

    VkCommandBuffer cmd_buffers[BEE_GPU_MAX_FRAMES_IN_FLIGHT];

    VkCommandBufferAllocateInfo cmd_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr };
    cmd_info.commandPool = command_pool;
    cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_info.commandBufferCount = BEE_GPU_MAX_FRAMES_IN_FLIGHT;

    BEE_VK_CHECK(vkAllocateCommandBuffers(device->handle, &cmd_info, cmd_buffers));

    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    for (auto& cmd : cmd_buffers)
    {
        BEE_VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));
    }

    VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
    fence_info.flags = 0; // unsignalled

    VkSemaphoreCreateInfo sem_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr };
    sem_info.flags = 0;

    for (int frame = 0; frame < BEE_GPU_MAX_FRAMES_IN_FLIGHT; ++frame)
    {
        auto& buffer = buffers[frame];
        new (&buffer) StagingBuffer{};

        buffer.cmd = cmd_buffers[frame];

        BEE_VK_CHECK(vkCreateFence(device->handle, &fence_info, nullptr, &buffer.submit_fence));
        BEE_VK_CHECK(vkCreateSemaphore(device->handle, &sem_info, nullptr, &buffer.transfer_semaphore));
    }
}

void VulkanStaging::destroy()
{
    BEE_ASSERT(device != VK_NULL_HANDLE);

    for (auto& buffer : buffers)
    {
        if (buffer.is_submitted)
        {
            BEE_VK_CHECK(vkWaitForFences(device->handle, 1, &buffer.submit_fence, VK_TRUE, limits::max<u64>()));
            BEE_VK_CHECK(vkResetFences(device->handle, 1, &buffer.submit_fence));
        }

        if (buffer.allocation != nullptr)
        {
            vmaUnmapMemory(vma_allocator, buffer.allocation);
            vmaDestroyBuffer(vma_allocator, buffer.handle, buffer.allocation);
        }
        buffer.handle = VK_NULL_HANDLE;

        vkDestroyFence(device->handle, buffer.submit_fence, nullptr);
        vkDestroySemaphore(device->handle, buffer.transfer_semaphore, nullptr);
    }

    BEE_VK_CHECK(vkResetCommandPool(device->handle, command_pool, 0));
    vkDestroyCommandPool(device->handle, command_pool, nullptr);
}

void VulkanStaging::ensure_capacity(const size_t capacity)
{
    if (capacity <= buffer_capacity)
    {
        return;
    }

    // Reallocate all frame staging buffers with new capacity
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
        for (int i = 0; i < static_array_length(buffers); ++i)
        {
            submit_frame(i);
        }

        for (int i = 0; i < static_array_length(buffers); ++i)
        {
            wait_on_frame(i);
        }

        ensure_capacity(size);
    }

    auto* buffer = &buffers[current_frame];
    chunk->offset = round_up(buffer->offset, alignment);

    // flip to the next staging buffer if this chunk is about to exceed its capacity
    if (chunk->offset + size >= buffer_capacity && !buffer->is_submitted)
    {
        current_frame = submit_frame(current_frame);
        wait_on_frame(current_frame); // wait for the new staging buffer to finish
        buffer = &buffers[current_frame];
        chunk->offset = 0;
    }

    // assign all the out parameters to the chunk
    chunk->data = reinterpret_cast<u8*>(buffer->data) + chunk->offset;
    chunk->cmd = buffer->cmd;
    chunk->buffer = buffer->handle;

    // increment the buffers offset
    buffer->offset = chunk->offset + size;
}


} // namespace bee
