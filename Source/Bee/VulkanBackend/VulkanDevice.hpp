/*
 *  VulkanObjects.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Memory/ChunkAllocator.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"

#include "Bee/VulkanBackend/VulkanObjectCache.hpp"
#include "Bee/Gpu/ResourceTable.hpp"

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

    void submit(VulkanDevice* device);
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
    PixelFormat                     selected_format;

    StaticString<16>                id_string;
};

struct VulkanCommandPool;
struct VulkanPipelineState;
struct VulkanResourceBinding;

struct VulkanRenderPass
{
    RenderPassHandle        lookup_handle;
    u32                     hash { 0 };
    RenderPassCreateInfo    create_info;
    VkRenderPass            handle { VK_NULL_HANDLE };
};


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
    VulkanRenderPass*       current_render_pass { nullptr };
    VkDescriptorSet         descriptors[BEE_GPU_MAX_RESOURCE_LAYOUTS];
    const void*             push_constants[ShaderStageIndex::count];

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
    TextureCreateInfo   create_info;
    VmaAllocation       allocation { VK_NULL_HANDLE };
    VmaAllocationInfo   allocation_info;
    VkImage             handle { VK_NULL_HANDLE };
    i32                 swapchain { -1 };
    VkImageLayout       layout { VK_IMAGE_LAYOUT_UNDEFINED };
};

struct VulkanTextureView
{
    VkImageView     handle { VK_NULL_HANDLE };
    TextureHandle   viewed_texture;
    PixelFormat     format { PixelFormat::unknown };
    u32             samples { 0 };
    i32             swapchain { -1 };
};

struct VulkanShader
{
    u32                 hash { 0 };
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
    const char*         debug_name { nullptr };

    VulkanBuffer() = default;

    VulkanBuffer(const BufferType new_type, const DeviceMemoryUsage new_usage, const u32 new_size)
        : type(new_type),
          usage(new_usage),
          size(new_size)
    {}

    inline bool is_dynamic() const
    {
        return (type & BufferType::dynamic_buffer) != BufferType::unknown;
    }
};

struct VulkanBufferAllocation
{
    VkBuffer        handle { VK_NULL_HANDLE };
    VmaAllocation   allocation { VK_NULL_HANDLE };

    VulkanBufferAllocation() = default;

    VulkanBufferAllocation(VkBuffer new_handle, VmaAllocation new_allocation)
        : handle(new_handle),
          allocation(new_allocation)
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
    VkCommandBuffer     cmd[2] { VK_NULL_HANDLE }; // 0: transfer, 1: graphics
    VkBuffer            buffer { VK_NULL_HANDLE };
};

struct VulkanStaging
{
    enum Enum
    {
        transfer_index,
        graphics_index
    };

    BEE_ENUM_STRUCT(VulkanStaging)

    struct StagingBuffer
    {
        CommandBufferState  cmd_state { CommandBufferState::invalid };
        size_t              offset { 0 };
        void*               data { nullptr };
        VmaAllocation       allocation { VK_NULL_HANDLE };
        VmaAllocationInfo   allocation_info{};
        VkBuffer            handle { VK_NULL_HANDLE };
        VkCommandBuffer     cmd[2] { VK_NULL_HANDLE };
        VkFence             submit_fence[2] { VK_NULL_HANDLE };
        VkSemaphore         semaphores[2] { VK_NULL_HANDLE };

        void begin_commands();

        void end_commands();

        void submit_commands(VulkanDevice* device_ptr, VulkanQueue** queue_ptrs);

        void wait_commands(VulkanDevice* device_ptr);
    };

    StagingBuffer       buffers[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VkCommandPool       command_pool[2] { VK_NULL_HANDLE };
    size_t              buffer_capacity { 0 };
    i32                 current_buffer_index {0 };
    VulkanQueue*        queues[2] { nullptr };
    VulkanDevice*       device { nullptr };
    VmaAllocator        vma_allocator { VK_NULL_HANDLE };

    void init(VulkanDevice* new_device, VmaAllocator new_vma_allocator);

    void destroy();

    void allocate(const size_t size, const size_t alignment, VulkanStagingChunk* chunk);

    void submit();

    bool is_pending();
private:
    void ensure_capacity(const size_t capacity);
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
    u32                                     index { limits::max<u32>() };
    VulkanStaging                           staging;
    VulkanCommandPool                       command_pool[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VulkanDescriptorPoolCache               dynamic_descriptor_pools[BEE_GPU_MAX_FRAMES_IN_FLIGHT];
    VulkanDescriptorPoolCache               static_descriptor_pools;
    VulkanResourceBinding*                  static_resource_binding_pending_deletes { nullptr };
    DynamicArray<VulkanBufferAllocation>    dynamic_buffer_deletes[BEE_GPU_MAX_FRAMES_IN_FLIGHT];

    // Device commands and updates
    CommandBuffer*                          device_cmd[BEE_GPU_MAX_FRAMES_IN_FLIGHT];

    GpuResourceTable<TextureHandle, VulkanTexture>                  textures;
    GpuResourceTable<TextureViewHandle, VulkanTextureView>          texture_views;
    GpuResourceTable<BufferHandle, VulkanBuffer>                    buffers;
    GpuResourceTable<RenderPassHandle, VulkanRenderPass>            render_passes;
    GpuResourceTable<ShaderHandle, VulkanShader>                    shaders;
    GpuResourceTable<FenceHandle, VkFence>                          fences;
    GpuResourceTable<ResourceBindingHandle, VulkanResourceBinding>  resource_bindings;
    GpuResourceTable<SamplerHandle, VkSampler>                      samplers;
//            GpuResourceTable<VulkanTexture> buffer_views { sizeof(VulkanTexturee) * 64 ;

    VulkanThreadData() = default;

    VulkanThreadData(const u32 thread_index)
        : index(thread_index),
          textures(thread_index, sizeof(VulkanTexture) * 64),
          texture_views(thread_index, sizeof(VulkanTextureView) * 64),
          buffers(thread_index, sizeof(VulkanBuffer) * 64),
          render_passes(thread_index, sizeof(VulkanRenderPass) * 64),
          shaders(thread_index, sizeof(VulkanShader) * 64),
          fences(thread_index, sizeof(VkFence) * 64),
          resource_bindings(thread_index, sizeof(VulkanResourceBinding) * 64),
          samplers(thread_index, sizeof(VkSampler) * 64)
    {}

    void flush_deallocations()
    {
        textures.flush_deallocations();
        texture_views.flush_deallocations();
        buffers.flush_deallocations();
        render_passes.flush_deallocations();
        shaders.flush_deallocations();
        fences.flush_deallocations();
        resource_bindings.flush_deallocations();
        samplers.flush_deallocations();
    }

    CommandBuffer* get_device_cmd(const DeviceHandle device_handle);
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

    VulkanQueueSubmit               submissions[vk_max_queues];

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
    VulkanPendingCache<VulkanPipelineKey, VulkanPipelineState>          pipeline_cache;
    // TODO(Jacob): VkPipelineCache

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
        return thread_data[static_cast<i32>(object_handle.thread())];
    }

#define BEE_GPU_OBJECT_ACCESSOR(HANDLE_TYPE, T, TABLE_NAME)    \
    T& TABLE_NAME##_get(const HANDLE_TYPE obj_handle)          \
    {                                                          \
        return get_thread(obj_handle).TABLE_NAME[obj_handle];  \
    }

    BEE_GPU_OBJECT_ACCESSOR(TextureHandle, VulkanTexture, textures)
    BEE_GPU_OBJECT_ACCESSOR(TextureViewHandle, VulkanTextureView, texture_views)
    BEE_GPU_OBJECT_ACCESSOR(BufferHandle, VulkanBuffer, buffers)
    BEE_GPU_OBJECT_ACCESSOR(RenderPassHandle, VulkanRenderPass, render_passes)
    BEE_GPU_OBJECT_ACCESSOR(ShaderHandle, VulkanShader, shaders)
    BEE_GPU_OBJECT_ACCESSOR(FenceHandle, VkFence, fences)
    BEE_GPU_OBJECT_ACCESSOR(ResourceBindingHandle, VulkanResourceBinding, resource_bindings)
    BEE_GPU_OBJECT_ACCESSOR(SamplerHandle, VkSampler, samplers)
//    BEE_GPU_OBJECT_ACCESSOR(VulkanBuffer*, buffer_views)
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
    GpuBackend                          api;
    GpuCommandBackend                   command_backend;
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
VkSurfaceKHR vk_create_wsi_surface(VkInstance instance, void* os_window);

BEE_FORCE_INLINE i32 queue_type_index(const QueueType type)
{
    BEE_ASSERT(type != QueueType::none);
    return math::log2i(static_cast<u32>(type));
}

void create_texture_view_internal(VulkanDevice* device, const TextureViewCreateInfo& desc, VulkanTextureView* dst);

void allocate_dynamic_binding(VulkanDevice* device, VulkanResourceBinding* binding);

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