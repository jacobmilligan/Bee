/*
 *  VulkanObjects.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/ChunkAllocator.hpp"
#include "Bee/Plugins/Gpu/ResourceTable.hpp"
#include "Bee/Plugins/VulkanBackend/VulkanObjectCache.hpp"

#include <volk.h>

BEE_PUSH_WARNING
    BEE_DISABLE_WARNING_MSVC(4127) // conditional expression is constant
    BEE_DISABLE_WARNING_MSVC(4189) // local variable is initialized but not referenced
    BEE_DISABLE_WARNING_MSVC(4701) // potentially uninitialized local variable used
    BEE_DISABLE_WARNING_MSVC(4324) // 'VmaPoolAllocator<VmaAllocation_T>::Item': structure was padded due to alignment specifier
    #include <vk_mem_alloc.h>
BEE_POP_WARNING

#define BEE_VK_MAX_SWAPCHAINS 32


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

struct VulkanDevice;

/*
 ******************************************
 *
 * # Vulkan device objects
 *
 ******************************************
 */
static constexpr u32 vk_max_queues = 3;

struct VulkanQueue
{
    static constexpr u32 invalid_queue_index = limits::max<u32>();

    u32             index { invalid_queue_index };
    VkQueue         handle { VK_NULL_HANDLE };

    void submit(const VkSubmitInfo& submit_info, VkFence fence, VulkanDevice* device) const;

    VkResult present(const VkPresentInfoKHR* present_info, VulkanDevice* device) const;
};

struct VulkanQueueSubmit
{
    i32                             queue { -1 };
    VkSubmitInfo                    info { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr };
    DynamicArray<VkCommandBuffer>   cmd_buffers;

    void reset();

    void add(CommandBuffer* cmd);

    void submit(VulkanDevice* device, VkFence fence);
};

struct VulkanSwapchain
{
    VkSwapchainKHR                  handle { VK_NULL_HANDLE };
    VkSurfaceKHR                    surface { VK_NULL_HANDLE };
    VkSemaphore                     acquire_semaphore[BEE_GPU_MAX_FRAMES_IN_FLIGHT] { VK_NULL_HANDLE };
    VkSemaphore                     render_semaphore[BEE_GPU_MAX_FRAMES_IN_FLIGHT] { VK_NULL_HANDLE };

    RecursiveMutex                  mutex;
    bool                            pending_image_acquire { true };
    i32                             present_index { 0 };
    u32                             current_image { 0 };
    FixedArray<TextureHandle>       images;
    FixedArray<TextureViewHandle>   image_views;
    SwapchainCreateInfo             create_info;

    StaticString<16>                id_string;
};

struct VulkanCommandPool;
struct VulkanPipelineState;
struct VulkanResourceBinding;

struct CommandBuffer
{
    CommandBufferState      state { CommandBufferState::invalid };
    VulkanQueue*            queue { nullptr };
    VulkanDevice*           device { nullptr };
    VulkanCommandPool*      pool { nullptr };
    VkCommandBuffer         handle { VK_NULL_HANDLE };
    i32                     target_swapchain { -1 };

    // Draw state
    VulkanPipelineState*    bound_pipeline { nullptr };
    VkDescriptorSet         descriptors[BEE_GPU_MAX_RESOURCE_LAYOUTS];

    void reset(VulkanDevice* new_device);
};

struct VulkanCommandPool
{
    VkCommandPool   handle { VK_NULL_HANDLE };
    CommandBuffer   command_buffers[BEE_GPU_MAX_COMMAND_BUFFERS_PER_THREAD];
    i32             command_buffer_count { 0 };
};

struct VulkanTexture // NOLINT
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
    VkImage             handle { VK_NULL_HANDLE };
    i32                 swapchain { -1 };
};

struct VulkanTextureView
{
    VkImageView     handle { VK_NULL_HANDLE };
    TextureHandle   viewed_texture;
    PixelFormat     format { PixelFormat::unknown };
    u32             samples { 0 };
    i32             swapchain { -1 };
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

struct VulkanPipelineState
{
    VkPipeline          handle { VK_NULL_HANDLE };
    VkPipelineLayout    layout { VK_NULL_HANDLE };
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

struct VulkanThreadData;

struct VulkanDescriptorPool // NOLINT
{
    VulkanThreadData*               thread { nullptr };
    VkDescriptorPool                handle { VK_NULL_HANDLE };
    VkDescriptorSetLayout           layout { VK_NULL_HANDLE };
    u32                             allocated_sets { 0 };
    u32                             max_sets { 0 };
    u32                             size_count { 0 };
    VkDescriptorPoolSize            sizes[underlying_t(ResourceBindingType::unknown)];
};

struct VulkanResourceBinding
{
    i32                             allocated_frame { -1 };
    VulkanResourceBinding*          next { nullptr }; // for delete list
    ResourceBindingUpdateFrequency  update_frequency { ResourceBindingUpdateFrequency::persistent };
    ResourceLayoutDescriptor        layout;
    VkDescriptorSet                 set { VK_NULL_HANDLE };
    VulkanDescriptorPool*           pool { nullptr };
};

struct VulkanDescriptorPoolCache
{
    VulkanThreadData*                                                   thread { nullptr };
    DynamicHashMap<ResourceLayoutDescriptor, VulkanDescriptorPool*>     pools;
    DynamicArray<VkDescriptorPool>                                      to_destroy_pools;

    void clear_pending(VkDevice device);

    void destroy(VkDevice device);

    void reset(VkDevice device);
};

VulkanDescriptorPool* get_or_create_descriptor_pool(VulkanDevice* device, const ResourceBindingUpdateFrequency update_frequency, const ResourceLayoutDescriptor& layout);

/*
 ******************************************
 *
 * # Vulkan staging
 *
 ******************************************
 */
struct VulkanStagingChunk
{
    u8*                 data { nullptr };
    size_t              offset { 0 };
    VkCommandBuffer     cmd { VK_NULL_HANDLE };
    VkBuffer            buffer { VK_NULL_HANDLE };
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

    StagingBuffer       buffers[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VkCommandPool       command_pool { VK_NULL_HANDLE };
    size_t              buffer_capacity { 0 };
    i32                 current_frame { 0 };
    VulkanQueue*        transfer_queue { nullptr };
    VulkanDevice*       device { nullptr };
    VmaAllocator        vma_allocator { VK_NULL_HANDLE };

    void init(VulkanDevice* new_device, VulkanQueue* new_transfer_queue, VmaAllocator new_vma_allocator);

    void destroy();

    void allocate(const size_t size, const size_t alignment, VulkanStagingChunk* chunk);
private:
    void wait_on_frame(const i32 frame);

    void ensure_capacity(const size_t capacity);

    i32 submit_frame(const i32 frame);
};


/*
 ******************************************
 *
 * # Vulkan thread data
 *
 * Per-thread object data
 *
 ******************************************
 */
struct VulkanThreadData
{
    // Owned and allocated Vulkan objects
    i32                         index { -1 };
    ChunkAllocator              allocator;
    VulkanStaging               staging;
    u8*                         delete_list { nullptr };

    VulkanCommandPool           command_pool[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VulkanQueueSubmit           queue_submissions[vk_max_queues];
    VulkanDescriptorPoolCache   dynamic_descriptor_pools[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VulkanDescriptorPoolCache   static_descriptor_pools;
    VulkanResourceBinding*      static_resource_binding_pending_deletes { nullptr };

    union
    {
        GpuResourceTable        resource_tables[9];

        struct
        {
            GpuResourceTable            textures;
            GpuResourceTable            texture_views;
            GpuResourceTable            buffers;
//            GpuResourceTable buffer_views;
            GpuResourceTable            render_passes;
            GpuResourceTable            shaders;
            GpuResourceTable            pipeline_states;
            GpuResourceTable            fences;
            GpuResourceTable            resource_bindings;
            GpuResourceTable            samplers;
        };
    };

    template <typename T>
    void add_pending_delete(T* ptr)
    {
        destruct(ptr);

        reinterpret_cast<u8**>(ptr)[0] = nullptr;

        if (delete_list == nullptr)
        {
            delete_list = reinterpret_cast<u8*>(ptr);
        }
        else
        {
            reinterpret_cast<u8**>(delete_list)[0] = reinterpret_cast<u8*>(ptr);
        }
    }
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
    bool                            debug_markers_enabled { false };
    VkPhysicalDevice                physical_device { VK_NULL_HANDLE };
    VkDevice                        handle { VK_NULL_HANDLE };
    VkQueueFamilyProperties         queue_family_properties[vk_max_queues];
    VmaAllocator                    vma_allocator { VK_NULL_HANDLE };

    union
    {
        VulkanQueue                 queues[vk_max_queues];

        struct
        {
            VulkanQueue             graphics_queue;
            VulkanQueue             compute_queue;
            VulkanQueue             transfer_queue;
        };
    };

    RecursiveMutex                  per_queue_mutex[vk_max_queues];
    RecursiveMutex                  device_mutex;

    i32                             current_frame { 0 };
    u32                             present_queue { VulkanQueue::invalid_queue_index };
    FixedArray<VulkanThreadData>    thread_data;
    VulkanSwapchain                 swapchains[BEE_VK_MAX_SWAPCHAINS];

    // Cached objects
    VulkanPendingCache<VulkanPipelineLayoutKey, VkPipelineLayout>       pipeline_layout_cache;
    VulkanPendingCache<ResourceLayoutDescriptor, VkDescriptorSetLayout> descriptor_set_layout_cache;
    VulkanPendingCache<VulkanFramebufferKey, VkFramebuffer>             framebuffer_cache;

    // Fence pool
    RecursiveMutex          fence_mutex;
    DynamicArray<VkFence>   free_submit_fences[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    DynamicArray<VkFence>   used_submit_fences[BEE_GPU_MAX_FRAMES_IN_FLIGHT];

    VulkanDevice() // NOLINT
    {
        memset(queues, 0, sizeof(VulkanQueue) * vk_max_queues);
    }

    inline VulkanThreadData& get_thread()
    {
        return thread_data[job_worker_id()];
    }

    template <typename HandleType>
    inline VulkanThreadData& get_thread(const HandleType object_handle)
    {
        return thread_data[object_handle.thread()];
    }

#define BEE_GPU_OBJECT_ACCESSOR(T, table_name)                                          \
    GpuObjectHandle table_name##_add(T ptr)                                             \
    {                                                                                   \
        return get_thread().table_name.add(ptr);                                        \
    }                                                                                   \
    T table_name##_get(const GpuObjectHandle obj_handle)                                \
    {                                                                                   \
        return static_cast<T>(get_thread(obj_handle).table_name.get(obj_handle));       \
    }                                                                                   \
    T table_name##_remove(const GpuObjectHandle obj_handle)                             \
    {                                                                                   \
        return static_cast<T>(get_thread(obj_handle).table_name.remove(obj_handle));    \
    }

    BEE_GPU_OBJECT_ACCESSOR(VulkanTexture*, textures)
    BEE_GPU_OBJECT_ACCESSOR(VulkanTextureView*, texture_views)
    BEE_GPU_OBJECT_ACCESSOR(VulkanBuffer*, buffers)
//    BEE_GPU_OBJECT_ACCESSOR(VulkanBuffer*, buffer_views)
    BEE_GPU_OBJECT_ACCESSOR(VulkanRenderPass*, render_passes)
    BEE_GPU_OBJECT_ACCESSOR(VulkanShader*, shaders)
    BEE_GPU_OBJECT_ACCESSOR(VulkanPipelineState*, pipeline_states)
    BEE_GPU_OBJECT_ACCESSOR(VkFence, fences)
    BEE_GPU_OBJECT_ACCESSOR(VulkanResourceBinding*, resource_bindings)
    BEE_GPU_OBJECT_ACCESSOR(VkSampler, samplers)
};

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
struct VulkanBackend // NOLINT
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
        , VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

#ifdef VK_USE_PLATFORM_WIN32_KHR
        , VK_KHR_WIN32_SURFACE_EXTENSION_NAME
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
        "VK_LAYER_KHRONOS_validation",  // enables threading, parameter, object memory, core validation
        "VK_LAYER_LUNARG_monitor"       // displays FPS in title bar (maybe useless)
    };
#endif // BEE_CONFIG_ENABLE_ASSERTIONS

    ~VulkanBackend()
    {
        for (auto& device : devices)
        {
            BEE_ASSERT_F(device.handle == VK_NULL_HANDLE, "All GPU devices must be destroyed before the GPU backend is destroyed");
        }
    }
};

const char* vk_result_string(VkResult result);

// Implemented by platform-specific code
VkSurfaceKHR vk_create_wsi_surface(VkInstance instance, const WindowHandle& window);

BEE_FORCE_INLINE i32 queue_type_index(const QueueType type)
{
    BEE_ASSERT(type != QueueType::none);
    return math::log2i(static_cast<u32>(type));
}

bool create_texture_view_internal(VulkanDevice* device, const TextureViewCreateInfo& desc, VulkanTextureView* dst);

/*
 ******************************************
 *
 * # Vulkan cached objects
 *
 ******************************************
 */
VkDescriptorSetLayout get_or_create_descriptor_set_layout(VulkanDevice* device, const ResourceLayoutDescriptor& key);

/*
 ******************************************
 *
 * # Vulkan debug markers
 *
 * Not set in release builds - debug only
 *
 ******************************************
 */
void set_vk_object_tag(VulkanDevice* device, VkDebugReportObjectTypeEXT object_type, void* object, size_t tag_size, const void* tag);
void set_vk_object_name(VulkanDevice* device, VkDebugReportObjectTypeEXT object_type, void* object, const char* name);


} // namespace bee