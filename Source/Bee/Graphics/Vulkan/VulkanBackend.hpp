/*
 *  Vulkan.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Core/Containers/HandleTable.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Graphics/GPU.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"
#include "Bee/Core/Concurrency.hpp"

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
static constexpr u32 vk_max_queues = 3;

struct VulkanDevice;

struct VulkanQueue
{
    static constexpr u32 invalid_queue_index = limits::max<u32>();

    u32                 index { invalid_queue_index };
    VkQueue             handle { VK_NULL_HANDLE };
    RecursiveSpinLock*  mutex { nullptr };

    void submit_threadsafe(const u32 submit_count, const VkSubmitInfo* submits, VkFence fence);

    VkResult present_threadsafe(const VkPresentInfoKHR* present_info);
};


struct VulkanSwapchain
{
    VkSwapchainKHR                  handle { VK_NULL_HANDLE };
    VkSurfaceKHR                    surface { VK_NULL_HANDLE };
    VkSemaphore                     acquire_semaphore[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore                     render_semaphore[BEE_GPU_MAX_FRAMES_IN_FLIGHT];

    RecursiveSpinLock               mutex;
    bool                            pending_image_acquire { true };
    i32                             present_index { 0 };
    u32                             current_image { 0 };
    FixedArray<TextureHandle>       images;
    FixedArray<TextureViewHandle>   image_views;
    SwapchainCreateInfo             create_info;

    char                            id_string[16];
};

struct VulkanRenderPass
{
    RenderPassHandle        lookup_handle;
    RenderPassCreateInfo    create_info;
    VkRenderPass            handle { VK_NULL_HANDLE };
};

struct VulkanShader
{
    VkShaderModule      handle { VK_NULL_HANDLE };
    StaticString<256>   entry;
};

struct VulkanBuffer
{
    DeviceMemoryUsage   usage { DeviceMemoryUsage::unknown };
    BufferType          type { BufferType::unknown };
    u32                 size { 0 };
    VmaAllocation       allocation { VK_NULL_HANDLE };
    VmaAllocationInfo   allocation_info{};
    VkBuffer            handle { VK_NULL_HANDLE };
    VkAccessFlags       access { 0 };

    VulkanBuffer() = default;

    VulkanBuffer(const BufferType new_type, const DeviceMemoryUsage new_usage, const u32 new_size)
        : type(new_type),
          usage(new_usage),
          size(new_size)
    {}
};


struct VulkanTexture
{
    u32                 width { 0 };
    u32                 height { 0 };
    u32                 depth { 1 };
    u32                 layers { 0 };
    u32                 levels { 0 };
    u32                 samples { 0 };
    TextureUsage        usage { TextureUsage::unknown };
    PixelFormat         format { PixelFormat::unknown };
    VmaAllocation       allocation { VK_NULL_HANDLE };
    VmaAllocationInfo   allocation_info;
    VkImage             handle {VK_NULL_HANDLE };
    SwapchainHandle     swapchain;
};

struct VulkanTextureView
{
    VkImageView     handle { VK_NULL_HANDLE };
    TextureHandle   viewed_texture;
    PixelFormat     format { PixelFormat::unknown };
    u32             samples { 0 };
    SwapchainHandle swapchain;
};

struct VulkanDevice;
struct VulkanCommandPool;

struct NativeCommandBuffer
{
    i32                     index { -1 };
    i32                     queue { -1 };
    VkCommandBuffer         handle { VK_NULL_HANDLE };
    CommandPoolHandle       pool;
    VulkanDevice*           device { nullptr };
    SwapchainHandle         target_swapchain;
    CommandBuffer*          api { nullptr };
};

struct VulkanCommandPool
{
    struct PerQueuePool
    {
        VkCommandPool                       handle { VK_NULL_HANDLE };
        DynamicArray<NativeCommandBuffer*>     command_buffers;
    };

    PoolAllocator   allocator;
    PerQueuePool    per_queue_pools[3];
};


struct VulkanFramebufferKey
{
    struct FormatKey
    {
        PixelFormat format { PixelFormat::unknown };
        u32         sample_count { 0 };
    };

    u32       width { 0 };
    u32       height { 0 };
    u32       layers { 0 };
    u32       attachment_count { 0 };
    FormatKey attachments[BEE_GPU_MAX_ATTACHMENTS];
};

struct VulkanFramebuffer
{
    VkFramebuffer           handle { VK_NULL_HANDLE };
    VulkanFramebufferKey    key;
    VkImageView             image_views[BEE_GPU_MAX_ATTACHMENTS];
};

template <>
struct Hash<VulkanFramebufferKey>
{
    u32 inline operator()(const VulkanFramebufferKey& key) const
    {
        HashState hash;
        hash.add(key.width);
        hash.add(key.height);
        hash.add(key.layers);
        hash.add(key.attachment_count);
        hash.add(key.attachments, sizeof(VulkanFramebufferKey::FormatKey) * key.attachment_count);
        return hash.end();
    }
};


struct VulkanSubmission
{
    VkDevice                        device { VK_NULL_HANDLE };
    VkSubmitInfo                    info { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
    VkFence                         fence { VK_NULL_HANDLE };

    void wait()
    {
        if (device == VK_NULL_HANDLE || fence == VK_NULL_HANDLE || info.commandBufferCount == 0)
        {
            return;
        }

        vkWaitForFences(device, 1, &fence, VK_TRUE, limits::max<u64>());
        vkResetFences(device, 1, &fence);
    }

    void reset(VkDevice new_device)
    {
        device = new_device;

        if (fence == VK_NULL_HANDLE)
        {
            VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr };
            fence_info.flags = 0;
            BEE_VK_CHECK(vkCreateFence(device, &fence_info, nullptr, &fence));
        }

        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.pNext = nullptr;
        info.waitSemaphoreCount = 0;
        info.pWaitSemaphores = nullptr;
        info.pWaitDstStageMask = nullptr;
        info.commandBufferCount = 0;
        info.pCommandBuffers = nullptr;
        info.signalSemaphoreCount = 0;
        info.pSignalSemaphores = nullptr;
    }
};

struct VulkanPipelineLayoutKey
{
    u32                             resource_layout_count { 0 };
    const ResourceLayoutDescriptor* resource_layouts { nullptr };
    u32                             push_constant_range_count { 0 };
    const PushConstantRange*        push_constant_ranges { nullptr };
};

template <>
struct Hash<VulkanPipelineLayoutKey>
{
    inline u32 operator()(const VulkanPipelineLayoutKey& key) const
    {
        HashState hash;
        hash.add(key.resource_layout_count);
        for (u32 i = 0; i < key.resource_layout_count; ++i)
        {
            hash.add(key.resource_layouts[i].resource_count);
            hash.add(key.resource_layouts[i].resources, sizeof(ResourceDescriptor) * key.resource_layouts[i].resource_count);
        }
        hash.add(key.push_constant_range_count);
        hash.add(key.push_constant_ranges, sizeof(PushConstantRange) * key.push_constant_range_count);
        return hash.end();
    }
};

struct VulkanPipeline
{
    VkPipeline handle { VK_NULL_HANDLE };
};


/*
 ******************************************
 *
 * # Vulkan staging
 *
 ******************************************
 */

/*
 * DO NOT HOLD ONTO THESE. Temporary chunk of memory from a staging buffer. They also hold a lock
 * so holding onto this will cause deadlocks in the staging buffer - should only be used in the
 * smallest scope possible
 */
struct VulkanStagingChunk
{
    RecursiveSpinLock*  mutex { nullptr };
    u8*                 data { nullptr };
    size_t              offset { 0 };
    VkCommandBuffer     cmd { VK_NULL_HANDLE };
    VkBuffer            buffer { VK_NULL_HANDLE };

    inline void release()
    {
        if (mutex != nullptr)
        {
            mutex->unlock();
        }

        mutex = nullptr;
    }

    ~VulkanStagingChunk()
    {
        release();
    }
};

struct VulkanStaging
{
    struct StagingBuffer
    {
        bool                is_submitted { false };
        size_t              offset { 0 };
        void*               data { nullptr };
        VmaAllocation       allocation { VK_NULL_HANDLE };
        VmaAllocationInfo   allocation_info{};
        VkBuffer            handle { VK_NULL_HANDLE };
        VkCommandBuffer     cmd { VK_NULL_HANDLE };
        VkFence             submit_fence { VK_NULL_HANDLE };
        VkSemaphore         transfer_semaphore { VK_NULL_HANDLE };
    };

    RecursiveSpinLock   mutex;
    StagingBuffer       buffers[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VkCommandPool       command_pool { VK_NULL_HANDLE };
    size_t              buffer_capacity { 0 };
    i32                 current_frame { 0 };
    VulkanQueue*        transfer_queue { nullptr };
    VkDevice            device { VK_NULL_HANDLE };
    VmaAllocator        vma_allocator { VK_NULL_HANDLE };

    void init(VkDevice new_device, VulkanQueue* new_transfer_queue, VmaAllocator new_vma_allocator);

    void destroy();

    void allocate(const size_t size, const size_t alignment, VulkanStagingChunk* chunk);
private:
    void wait_nolock(const i32 frame);

    void ensure_capacity_nolock(const size_t capacity);

    i32 submit_nolock(const i32 frame);
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
static constexpr size_t swapchain_chunk_size = sizeof(VulkanSwapchain) * 2;
static constexpr size_t render_pass_chunk_size = sizeof(VulkanRenderPass) * 16;
static constexpr size_t shader_chunk_size = sizeof(VulkanShader) * 32;
static constexpr size_t buffer_chunk_size = sizeof(VulkanBuffer) * 32;
static constexpr size_t texture_chunk_size = sizeof(VulkanTexture) * 32;
static constexpr size_t texture_view_chunk_size = sizeof(VulkanTextureView) * 32;
static constexpr size_t command_pool_chunk_size = sizeof(VulkanCommandPool) * 4;
static constexpr size_t fence_chunk_size = sizeof(VkFence) * 32;
static constexpr size_t pipeline_chunk_size = sizeof(VulkanPipeline) * 32;

struct VulkanDevice
{
    using swapchain_table_t     = ThreadSafeResourcePool<SwapchainHandle, VulkanSwapchain>;
    using render_pass_table_t   = ThreadSafeResourcePool<RenderPassHandle, VulkanRenderPass>;
    using shader_table_t        = ThreadSafeResourcePool<ShaderHandle, VulkanShader>;
    using buffer_table_t        = ThreadSafeResourcePool<BufferHandle, VulkanBuffer>;
    using texture_table_t       = ThreadSafeResourcePool<TextureHandle, VulkanTexture>;
    using texture_view_table_t  = ThreadSafeResourcePool<TextureViewHandle, VulkanTextureView>;
    using command_pool_table_t  = ThreadSafeResourcePool<CommandPoolHandle, VulkanCommandPool>;
    using fence_table_t         = ThreadSafeResourcePool<FenceHandle, VkFence>;
    using pipeline_table_t      = ThreadSafeResourcePool<PipelineStateHandle, VulkanPipeline>;

    VulkanDevice() noexcept // NOLINT
        : scratch_allocator(mebibytes(4), system_allocator()),
          swapchains(swapchain_chunk_size),
          render_passes(render_pass_chunk_size),
          shaders(shader_chunk_size),
          buffers(buffer_chunk_size),
          textures(texture_chunk_size),
          texture_views(texture_view_chunk_size),
          command_pools(command_pool_chunk_size),
          fences(fence_chunk_size),
          pipelines(pipeline_chunk_size)
    {
        memset(queues, 0, sizeof(VulkanQueue) * vk_max_queues);
    }

    VkPhysicalDevice        physical_device { VK_NULL_HANDLE };
    VkDevice                handle { VK_NULL_HANDLE };
    VkQueueFamilyProperties queue_family_properties[vk_max_queues];
    VmaAllocator            vma_allocator { VK_NULL_HANDLE };
    LinearAllocator         scratch_allocator;

    union
    {
        VulkanQueue         queues[vk_max_queues];

        struct
        {
            VulkanQueue     graphics_queue;
            VulkanQueue     compute_queue;
            VulkanQueue     transfer_queue;
        };
    };

    RecursiveSpinLock       per_queue_mutex[vk_max_queues];

    i32                     current_frame { 0 };
    u32                     present_queue { VulkanQueue::invalid_queue_index };

    // Owned and allocated Vulkan objects
    swapchain_table_t       swapchains;
    render_pass_table_t     render_passes;
    shader_table_t          shaders;
    buffer_table_t          buffers;
    texture_table_t         textures;
    texture_view_table_t    texture_views;
    command_pool_table_t    command_pools;
    fence_table_t           fences;
    pipeline_table_t        pipelines;

    std::atomic_int32_t     submit_queue_tail { 0 };
    VulkanSubmission        submit_queue[BEE_GPU_MAX_FRAMES_IN_FLIGHT][BEE_GPU_SUBMIT_QUEUE_SIZE];

    // Cached objects
    using framebuffer_bucket_t = DynamicArray<VulkanFramebuffer>;
    ReaderWriterMutex                           framebuffer_cache_mutex;
    DynamicHashMap<u32, framebuffer_bucket_t>   framebuffer_cache;
    DynamicHashMap<u32, VkPipelineLayout>       pipeline_layout_cache;
    DynamicHashMap<u32, VkDescriptorSetLayout>  descriptor_set_layout_cache;

    // Staging memory
    VulkanStaging                               staging;
};


VulkanSubmission* enqueue_submission(VulkanDevice* device);

/*
 ******************************************
 *
 * # Vulkan cached objects
 *
 ******************************************
 */
VkFramebuffer get_or_create_framebuffer(
    VulkanDevice* device,
    const VulkanFramebufferKey& key,
    VkRenderPass compatible_render_pass,
    const VkImageView* attachments
);

VkDescriptorSetLayout get_or_create_descriptor_set_layout(VulkanDevice* device, const ResourceLayoutDescriptor& key);

VkPipelineLayout get_or_create_pipeline_layout(VulkanDevice* device, const VulkanPipelineLayoutKey& key);

/*
 ******************************************
 *
 * # Vulkan backend
 *
 * Owns the Vulkan instance, all devices,
 * and the command buffer API
 *
 ******************************************
 */
struct VulkanBackend
{
    VkInstance                          instance { nullptr };

    i32                                 physical_device_count { 0 };
    VkPhysicalDevice                    physical_devices[BEE_GPU_MAX_PHYSICAL_DEVICES];
    VkPhysicalDeviceProperties          physical_device_properties[BEE_GPU_MAX_PHYSICAL_DEVICES];
    VkPhysicalDeviceMemoryProperties    physical_device_memory_properties[BEE_GPU_MAX_PHYSICAL_DEVICES];

    // There are never more than a few devices active at a time so we don't need to use handle pools.
    // Using a raw array avoids having to do unnecessary bitmask operations or version checking.
    // The difference here being that devices will be allowed to have an ID of zero
    VulkanDevice                        devices[BEE_GPU_MAX_DEVICES];

    static constexpr const char* required_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
        ,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

#ifdef VK_USE_PLATFORM_WIN32_KHR
        ,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif // #if VK_USE_PLATFORM_WIN32_KHR
    };

    static constexpr const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME // Require swapchain support for all devices
        ,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME // Enables negative viewport height & VK_ERROR_OUT_OF_POOL_MEMORY_KHR for clearer error reporting when doing vkAllocateDescriptorSets
#ifdef BEE_DEBUG
        ,
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME
#endif // BEE_DEBUG
    };

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    VkDebugReportCallbackEXT debug_report_cb { VK_NULL_HANDLE };

    static constexpr const char* enabled_validation_layers[] = {
        "VK_LAYER_LUNARG_standard_validation", // enables threading, parameter, object memory, core validation
        "VK_LAYER_LUNARG_monitor"              // displays FPS in title bar (maybe useless)
    };
#endif // BEE_CONFIG_ENABLE_ASSERTIONS

    ~VulkanBackend()
    {
        for (auto& device : devices)
        {
            BEE_ASSERT_F(device.handle == nullptr, "All GPU devices must be destroyed before the GPU backend is destroyed");
        }
    }
};


// Implemented by platform-specific code
VkSurfaceKHR vk_create_wsi_surface(VkInstance instance, const WindowHandle& window);

VulkanDevice& validate_device(const DeviceHandle& device);

BEE_FORCE_INLINE i32 queue_type_index(const QueueType type)
{
    BEE_ASSERT(type != QueueType::none);
    return math::log2i(static_cast<u32>(type));
}

TextureViewHandle vk_create_texture_view(VulkanDevice* device, const TextureViewCreateInfo& create_info);


/*
 ******************************************
 *
 * # Vulkan debug markers
 *
 * Not set in release builds - debug only
 *
 ******************************************
 */
void set_vk_object_tag(VkDevice device, VkDebugReportObjectTypeEXT object_type, void* object, size_t tag_size, const void* tag);

void set_vk_object_name(VkDevice device, VkDebugReportObjectTypeEXT object_type, void* object, const char* name);


} // namespace bee