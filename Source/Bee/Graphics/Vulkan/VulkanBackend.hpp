/*
 *  Vulkan.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Graphics/GPU.hpp"
#include "Bee/Core/Containers/HandleTable.hpp"

#include <volk.h>

BEE_PUSH_WARNING
    BEE_DISABLE_WARNING_MSVC(4127) // conditional expression is constant
    BEE_DISABLE_WARNING_MSVC(4189) // local variable is initialized but not referenced
    BEE_DISABLE_WARNING_MSVC(4701) // potentially uninitialized local variable used
    BEE_DISABLE_WARNING_MSVC(4324) // 'VmaPoolAllocator<VmaAllocation_T>::Item': structure was padded due to alignment specifier

    #include <vk_mem_alloc.h>
BEE_POP_WARNING


namespace bee {


/*
 ********************************************************************
 *
 * # Vulkan error handling
 *
 * Checks vulkan and VMA calls that their VkResult return
 * values == VK_SUCCESS and asserts if they aren't valid values
 * alongside an error message for that particular result
 *
 ********************************************************************
 */
#define BEE_VK_CHECK(fn) BEE_BEGIN_MACRO_BLOCK                                                  \
            const auto result = fn;                                                             \
            BEE_ASSERT_F(result == VK_SUCCESS, "Vulkan: %s", bee::vk_result_string(result));    \
        BEE_END_MACRO_BLOCK

#define BEE_VMA_CHECK(fn) BEE_BEGIN_MACRO_BLOCK                                                                                     \
            const auto result = fn;                                                                                                 \
            BEE_ASSERT_F(result != VK_ERROR_VALIDATION_FAILED_EXT, "Vulkan Memory Allocator tried to allocate zero-sized memory");  \
            BEE_ASSERT_F(result == VK_SUCCESS, "Vulkan: %s", bee::vk_result_string(result));                                        \
        BEE_END_MACRO_BLOCK


const char* vk_result_string(VkResult result);


/*
 ***************************************************************************************
 *
 * # Vulkan device objects
 *
 * Objects that are owned by a Vulkan logical device such as buffers, textures,
 * swapchains etc.
 *
 **************************************************************************************
 */
struct VulkanQueue
{
    static constexpr u32 invalid_queue_index = limits::max<u32>();

    u32     index { invalid_queue_index };
    VkQueue handle { VK_NULL_HANDLE };
};


struct VulkanSwapchain
{
    VkSwapchainKHR                  handle { VK_NULL_HANDLE };
    VkSurfaceKHR                    surface { VK_NULL_HANDLE };
    VkSemaphore                     image_available_sem[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore                     render_done_sem[BEE_GPU_MAX_FRAMES_IN_FLIGHT];

    i32                             current_image { 0 };
    FixedArray<TextureHandle>       images;
    FixedArray<TextureViewHandle>   image_views;
};


struct VulkanTexture
{
    u32                 width { 0 };
    u32                 height { 0 };
    u32                 layers { 0 };
    u32                 levels { 0 };
    u32                 samples { 0 };
    VkImageLayout       current_layout { VK_IMAGE_LAYOUT_UNDEFINED };
    TextureUsage        usage { TextureUsage::unknown };
    PixelFormat         format { PixelFormat::unknown };
    VmaAllocation       allocation { VK_NULL_HANDLE };
    VmaAllocationInfo   allocation_info;
    VkImage             handle { VK_NULL_HANDLE };
    bool                is_swapchain_image { false };
};

struct VulkanTextureView
{
    VkImageView     handle { VK_NULL_HANDLE };
    TextureHandle   viewed_texture;
    bool            is_swapchain_view { false };
};


/*
 ******************************************
 *
 * # Vulkan device
 *
 * Owns most vulkan objects and memory -
 * abstraction for a VkDevice
 *
 ******************************************
 */
struct VulkanDevice
{
    static constexpr u32 max_queues = 3;

    using swapchain_table_t         = HandleTable<BEE_GPU_MAX_SWAPCHAINS, SwapchainHandle, VulkanSwapchain>;
    using texture_table_t           = HandleTable<BEE_GPU_MAX_TEXTURES, TextureHandle, VulkanTexture>;
    using texture_view_table_t      = HandleTable<BEE_GPU_MAX_TEXTURE_VIEWS, TextureViewHandle, VulkanTextureView>;

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
        VulkanQueue         queues[max_queues];

        struct
        {
            VulkanQueue     graphics_queue;
            VulkanQueue     transfer_queue;
            VulkanQueue     compute_queue;
        };
    };

    u32                     present_queue { VulkanQueue::invalid_queue_index };

    // Owned and allocated Vulkan objects
    swapchain_table_t       swapchains;
    texture_table_t         textures;
    texture_view_table_t    texture_views;
};


// Implemented by platform-specific code
VkSurfaceKHR gpu_create_wsi_surface(VkInstance instance, const WindowHandle& window);


/*
 ******************************************
 *
 * # Vulkan debug markers
 *
 * Not set in release builds - debug only
 *
 ******************************************
 */
void gpu_set_object_tag(VkDevice device, VkDebugReportObjectTypeEXT object_type, void* object, size_t tag_size, const void* tag);

void gpu_set_object_name(VkDevice device, VkDebugReportObjectTypeEXT object_type, void* object, const char* name);


} // namespace bee