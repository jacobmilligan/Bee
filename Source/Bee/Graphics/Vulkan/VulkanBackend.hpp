/*
 *  Vulkan.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Graphics/GPU.hpp"

#if BEE_OS_WINDOWS == 1
    #define VK_USE_PLATFORM_WIN32_KHR
#endif // BEE_OS_WINDOWS == 1

#include <volk.h>

BEE_PUSH_WARNING
    BEE_DISABLE_WARNING_MSVC(4127) // conditional expression is constant
    BEE_DISABLE_WARNING_MSVC(4189) // local variable is initialized but not referenced
    BEE_DISABLE_WARNING_MSVC(4701) // potentially uninitialized local variable used
    BEE_DISABLE_WARNING_MSVC(4324) // 'VmaPoolAllocator<VmaAllocation_T>::Item': structure was padded due to alignment specifier

    #include <vk_mem_alloc.h>
BEE_POP_WARNING


namespace bee {


struct VulkanQueue
{
    static constexpr u32 invalid_queue_index = limits::max<u32>();

    u32     index { invalid_queue_index };
    VkQueue handle { VK_NULL_HANDLE };
};

struct VulkanDevice
{
    static constexpr u32 max_queues = 3;

    VulkanDevice()
    {
        memset(queues, 0, sizeof(VulkanQueue) * max_queues);
    }

    VkPhysicalDevice        physical_device;
    VkDevice                handle { VK_NULL_HANDLE };
    VkQueueFamilyProperties queue_family_properties[max_queues];
    VmaAllocator            vma_allocator { VK_NULL_HANDLE };

    union
    {
        VulkanQueue queues[max_queues];

        struct
        {
            VulkanQueue graphics_queue;
            VulkanQueue transfer_queue;
            VulkanQueue compute_queue;
        };
    };
};


} // namespace bee