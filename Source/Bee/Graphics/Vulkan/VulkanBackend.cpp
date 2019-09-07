/*
 *  Vulkan.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


#include "Bee/Core/Debug.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Graphics/Vulkan/VulkanBackend.hpp"
#include "Bee/Graphics/Vulkan/VulkanConvert.hpp"


namespace bee {


/*
 ************************************************
 *
 * # Vulkan helper functions and debug callbacks
 *
 ************************************************
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

const char* vk_result_string(VkResult result)
{
#define BEE_VKRESULT_NAME(r, str) case r : return str;
    switch (result) {
        BEE_VKRESULT_NAME(VK_SUCCESS, "Success")
        BEE_VKRESULT_NAME(VK_NOT_READY, "Not ready")
        BEE_VKRESULT_NAME(VK_TIMEOUT, "Timeout")
        BEE_VKRESULT_NAME(VK_EVENT_SET, "Event set")
        BEE_VKRESULT_NAME(VK_EVENT_RESET, "Event rest")
        BEE_VKRESULT_NAME(VK_INCOMPLETE, "Incomplete")
        BEE_VKRESULT_NAME(VK_ERROR_OUT_OF_HOST_MEMORY, "Out of host memory")
        BEE_VKRESULT_NAME(VK_ERROR_OUT_OF_DEVICE_MEMORY, "Out of device memory")
        BEE_VKRESULT_NAME(VK_ERROR_INITIALIZATION_FAILED, "Initialization failed")
        BEE_VKRESULT_NAME(VK_ERROR_DEVICE_LOST, "GraphicsDevice lost")
        BEE_VKRESULT_NAME(VK_ERROR_MEMORY_MAP_FAILED, "Memory map failed")
        BEE_VKRESULT_NAME(VK_ERROR_LAYER_NOT_PRESENT, "Layer not present")
        BEE_VKRESULT_NAME(VK_ERROR_EXTENSION_NOT_PRESENT, "Extension not present")
        BEE_VKRESULT_NAME(VK_ERROR_FEATURE_NOT_PRESENT, "Feature not present")
        BEE_VKRESULT_NAME(VK_ERROR_INCOMPATIBLE_DRIVER, "Incompatible driver")
        BEE_VKRESULT_NAME(VK_ERROR_TOO_MANY_OBJECTS, "Too many objects")
        BEE_VKRESULT_NAME(VK_ERROR_FORMAT_NOT_SUPPORTED, "Format not supported")
        BEE_VKRESULT_NAME(VK_ERROR_FRAGMENTED_POOL, "Fragmented pool")
        BEE_VKRESULT_NAME(VK_ERROR_OUT_OF_POOL_MEMORY, "Out of pool memory")
        BEE_VKRESULT_NAME(VK_ERROR_INVALID_EXTERNAL_HANDLE, "Invalid external handle")
        BEE_VKRESULT_NAME(VK_ERROR_SURFACE_LOST_KHR, "Surface lost")
        BEE_VKRESULT_NAME(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR, "Native window in use")
        BEE_VKRESULT_NAME(VK_SUBOPTIMAL_KHR, "Suboptimal")
        BEE_VKRESULT_NAME(VK_ERROR_OUT_OF_DATE_KHR, "Out of date")
        BEE_VKRESULT_NAME(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR, "Incompatible display")
        BEE_VKRESULT_NAME(VK_ERROR_VALIDATION_FAILED_EXT, "Validation failed")
        BEE_VKRESULT_NAME(VK_ERROR_INVALID_SHADER_NV, "Invalid shader")
        BEE_VKRESULT_NAME(VK_ERROR_FRAGMENTATION_EXT, "Fragmentation")
        BEE_VKRESULT_NAME(VK_ERROR_NOT_PERMITTED_EXT, "Not permitted")
        BEE_VKRESULT_NAME(VK_RESULT_RANGE_SIZE, "Range size")
        default:
            break;
    }

    return "Unknown error";

#undef BEE_VKRESULT_NAME
}

VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT object_type,
    u64 /*object*/,
    size_t location,
    i32 msg_code,
    const char* layer_prefix,
    const char* msg,
    void* /*user_data*/
)
{
    static constexpr const char* object_names[] =
        {
            "Unknown", // VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT = 0,
            "Instance", // VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT = 1,
            "PhysicalDevice", // VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT = 2,
            "Device", // VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT = 3,
            "Queue", // VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT = 4,
            "Semaphore", // VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT = 5,
            "CommpiandBuffer", // VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT = 6,
            "Fence", // VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT = 7,
            "DeviceMemory", // VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT = 8,
            "Buffer", // VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT = 9,
            "Image", // VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT = 10,
            "Event", // VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT = 11,
            "QueryPool", // VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT = 12,
            "BufferView", // VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_VIEW_EXT = 13,
            "ImageView", // VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT = 14,
            "ShaderModule", // VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT = 15,
            "PipelineCache", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT = 16,
            "PipelineLayout", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT = 17,
            "RenderPass", // VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT = 18,
            "Pipeline", // VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT = 19,
            "DescriptorSetLayout", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT = 20,
            "Sampler", // VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT = 21,
            "DescriptorPool", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT = 22,
            "DescriptorSet", // VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT = 23,
            "Framebuffer", // VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT = 24,
            "CommandPool", // VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT = 25,
            "SurfaceKHR", // VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT = 26,
            "SwapchainKHR", // VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT = 27,
            "DebugReportCallback", // VK_DEBUG_REPORT_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT_EXT = 28,
            "DisplayKHR", // VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_KHR_EXT = 29,
            "DisplayModeKHR", // VK_DEBUG_REPORT_OBJECT_TYPE_DISPLAY_MODE_KHR_EXT = 30,
            "ObjectTableNVX", // VK_DEBUG_REPORT_OBJECT_TYPE_OBJECT_TABLE_NVX_EXT = 31,
            "IndirectCommandsLayoutNVX", // VK_DEBUG_REPORT_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX_EXT = 32,
            "ValidationCache", // VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT = 33,
        };

    static const auto get_flag_string = [&]() -> const char*
    {
        if ((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0)
        {
            return "Info";
        }

        if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0)
        {
            return "Warning";
        }

        if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0)
        {
            return "Performance warning";
        }

        if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0)
        {
            return "Error";
        }

        if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0)
        {
            return "Debug";
        }

        return "Uknown";
    };

    BEE_ERROR("Vulkan validation", "%s (%s) %s: %d: %s\n", layer_prefix, object_names[object_type],
              get_flag_string(), msg_code, msg);
    log_stack_trace(LogVerbosity::error, 9);
    BEE_DEBUG_BREAK();
    return VK_FALSE;
}


/*
 **************************************************
 *
 * # GPU backend
 *
 * Contains the vulkan instance, physical device
 * data, and extensions/debug layers required
 *
 **************************************************
 */
static struct VulkanBackend
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

    static constexpr const char* required_extensions[] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
        , VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

#if BEE_OS_WINDOWS == 1
        , VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif // #if BEE_OS_WINDOWS == 1
    };

    static constexpr const char* device_extensions[] =
    {
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME
    };

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    VkDebugReportCallbackEXT debug_report_cb { VK_NULL_HANDLE };

    static constexpr const char* enabled_validation_layers[] =
    {
        "VK_LAYER_LUNARG_standard_validation",  // enables threading, parameter, object memory, core validation
        "VK_LAYER_LUNARG_monitor"               // displays FPS in title bar (maybe useless)
    };
#endif // BEE_CONFIG_ENABLE_ASSERTIONS
} g_backend;

#define BEE_GPU_VALIDATE_BACKEND() BEE_ASSERT_F(g_backend.instance != nullptr, "GPU backend has not been initialized")


BEE_FORCE_INLINE VulkanDevice& validate_device(const DeviceHandle& device)
{
    BEE_GPU_VALIDATE_BACKEND();
    BEE_ASSERT_F(
        device.id < BEE_GPU_MAX_DEVICES && g_backend.devices[device.id].handle != VK_NULL_HANDLE,
        "GPU device has an invalid ID or is destroyed/uninitialized"
    );
    return g_backend.devices[device.id];
}



/*
 ****************************************
 *
 * # GPU backend API - implementation
 *
 ****************************************
 */
bool gpu_init()
{
    if (BEE_FAIL_F(g_backend.instance == nullptr, "GPU backend is already initialized"))
    {
        return false;
    }

    // Initialize volk extension loader
    const auto volk_result = volkInitialize();
    if (volk_result != VK_SUCCESS)
    {
        log_error("Unable to initialize Vulkan - failed to find the Vulkan loader: %s", vk_result_string(volk_result));
        return false;
    }

    VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.pApplicationName = "Bee App";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Bee";
    app_info.engineVersion = VK_MAKE_VERSION(BEE_VERSION_MAJOR, BEE_VERSION_MINOR, BEE_VERSION_PATCH);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = static_array_length(g_backend.required_extensions);
    instance_info.ppEnabledExtensionNames = g_backend.required_extensions;
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    instance_info.enabledLayerCount = static_array_length(g_backend.enabled_validation_layers);
    instance_info.ppEnabledLayerNames = g_backend.enabled_validation_layers;
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    BEE_VK_CHECK(vkCreateInstance(&instance_info, nullptr, &g_backend.instance)); // create instance
    volkLoadInstance(g_backend.instance); // load all extensions

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    // Setup debug validation callbacks
    VkDebugReportCallbackCreateInfoEXT debug_cb_info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
    debug_cb_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debug_cb_info.pfnCallback = vk_debug_callback;

    BEE_VK_CHECK(vkCreateDebugReportCallbackEXT(g_backend.instance, &debug_cb_info, nullptr, &g_backend.debug_report_cb));
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    // Get all available physical devices up to MAX_PHYSICAL_DEVICES
    u32 device_count = 0;
    BEE_VK_CHECK(vkEnumeratePhysicalDevices(g_backend.instance, &device_count, nullptr));
    BEE_ASSERT_F(device_count > 0, "Unable to detect any supported physical graphics devices");

    // Get the physical device info for all available devices regardless of whether they're suitable or not
    device_count = math::min(device_count, BEE_GPU_MAX_PHYSICAL_DEVICES);
    BEE_VK_CHECK(vkEnumeratePhysicalDevices(g_backend.instance, &device_count, g_backend.physical_devices));

    // Get info for devices to allow user to select a device later
    for (u32 pd = 0; pd < device_count; ++pd)
    {
        auto& vk_pd = g_backend.physical_devices[pd];
        vkGetPhysicalDeviceMemoryProperties(vk_pd, &g_backend.physical_device_memory_properties[pd]);
        vkGetPhysicalDeviceProperties(vk_pd, &g_backend.physical_device_properties[pd]);
    }

    g_backend.physical_device_count = sign_cast<i32>(device_count);

    return true;
}

void gpu_destroy()
{
    for (auto& device : g_backend.devices)
    {
        BEE_ASSERT_F(device.handle == nullptr, "All GPU devices must be destroyed before the GPU backend is destroyed");
    }

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    vkDestroyDebugReportCallbackEXT(g_backend.instance, g_backend.debug_report_cb, nullptr);
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    vkDestroyInstance(g_backend.instance, nullptr);
    g_backend.instance = nullptr;
}

i32 gpu_enumerate_physical_devices(PhysicalDeviceInfo* dst_buffer, const i32 buffer_size)
{
    if (dst_buffer == nullptr)
    {
        return g_backend.physical_device_count;
    }

    const auto device_count = math::min(buffer_size, g_backend.physical_device_count);

    for (int pd = 0; pd < device_count; ++pd)
    {
        auto& props = g_backend.physical_device_properties[pd];
        auto& info = dst_buffer[pd];
        str::copy(info.name, PhysicalDeviceInfo::max_name_size, props.deviceName, static_array_length(props.deviceName));

        info.id = pd;
        info.type = convert_device_type(props.deviceType);
        info.vendor = convert_vendor(props.vendorID);

        const auto major = VK_VERSION_MAJOR(props.apiVersion);
        const auto minor = VK_VERSION_MINOR(props.apiVersion);
        const auto patch = VK_VERSION_PATCH(props.apiVersion);
        str::format(info.api_version, static_array_length(info.api_version), "Vulkan %u.%u.%u", major, minor, patch);
    }

    return device_count;
}


/*
 ****************************************
 *
 * # GPU Device - implementation
 *
 ****************************************
 */
DeviceHandle gpu_create_device(const DeviceCreateInfo& create_info)
{
    BEE_GPU_VALIDATE_BACKEND();

    if (g_backend.instance == VK_NULL_HANDLE)
    {
        log_error("Failed to create GPU device: Vulkan instance was VK_NULL_HANDLE");
        return DeviceHandle{};
    }

    const auto is_valid_physical_device_id = create_info.physical_device_id >= 0
                                          && create_info.physical_device_id < g_backend.physical_device_count;
    if (BEE_FAIL_F(is_valid_physical_device_id, "Invalid physical device ID specified in `DeviceCreateInfo`"))
    {
        return DeviceHandle{};
    }

    int device_idx = index_in_container(g_backend.devices, [](const VulkanDevice& d) { return d.handle == VK_NULL_HANDLE; });
    if (BEE_FAIL_F(device_idx >= 0, "Cannot create a new GPU device: Allocated devices has reached BEE_GPU_MAX_DEVICES"))
    {
        return DeviceHandle{};
    }

    auto physical_device = g_backend.physical_devices[create_info.physical_device_id];

    // Query the amount of extensions supported by the GPU
#ifdef BEE_VULKAN_DEVICE_EXTENSIONS_ENABLED
    u32 ext_count = 0;
    BEE_VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, nullptr));

    // Get the actual extension properties for the GPU
    auto supported_extensions = FixedArray<VkExtensionProperties>::with_size(sign_cast<int>(ext_count), temp_allocator());
    BEE_VK_CHECK(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &ext_count, supported_extensions.data()));
    // TODO(Jacob): iterate desired extensions here when possible
#else
    DynamicArray<const char*> device_extensions(temp_allocator());
    for (const auto& ext : g_backend.device_extensions)
    {
        device_extensions.push_back(ext);
    }
#endif // BEE_VULKAN_DEVICE_EXTENSIONS_ENABLED

    auto& device = g_backend.devices[device_idx];

    // Find all available queue families and store in device data for later use
    u32 available_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &available_queue_families, nullptr);
    available_queue_families = math::min(available_queue_families, VulkanDevice::max_queues);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &available_queue_families, device.queue_family_properties);

    memset(device.queues, 0, sizeof(VulkanQueue) * available_queue_families);

     /*
     * This function looks for a matching queue that has the lowest functionality available to allow using
     * it in the most specialized way possible
     */
    auto find_queue_index = [](const VulkanDevice& device, const VkQueueFlags type) -> u32
    {
        int lowest_count = limits::max<int>();
        int best_so_far = limits::max<u32>();

        for (u32 q = 0; q < device.max_queues; ++q)
        {
            if ((device.queue_family_properties[q].queueFlags & type) == 0)
            {
                continue;
            }

            int supported_count = 1;
            for_each_flag(device.queue_family_properties[q].queueFlags, [&](const VkQueueFlags flag)
            {
                ++supported_count;
            });

            if (supported_count < lowest_count)
            {
                lowest_count = supported_count;
                best_so_far = q;
            }
        }

        return best_so_far;
    };

    /*
     * We want the graphics queue to also double as a combined queue for gfx, compute, & transfer so here
     * we search for a queue matching GRAPHICS_BIT | COMPUTE_BIT because according to the spec
     * (Section 4.1 in the discussion of VkQueueFlagBits):
     *
     * 'If an implementation exposes any queue family that supports graphics operations, at least
     *  one queue family of at least one physical device exposed by the implementation must support
     *  **both** graphics and compute operations'
     *
     * Therefore, we can safely assume that if graphics is supported so is a generic graphics/compute
     * queue. Also any queue that defines graphics or compute operations also implicitly guarantees
     * transfer operations - so all of these calls should return valid queue indexes
     */
    device.graphics_queue.index = find_queue_index(device, VK_QUEUE_GRAPHICS_BIT);
    device.transfer_queue.index = find_queue_index(device, VK_QUEUE_TRANSFER_BIT);
    device.compute_queue.index = find_queue_index(device, VK_QUEUE_COMPUTE_BIT);

    BEE_ASSERT(device.graphics_queue.index < VulkanQueue::invalid_queue_index);

    int queue_info_indices[VulkanDevice::max_queues];
    VkDeviceQueueCreateInfo queue_infos[VulkanDevice::max_queues];

    memset(queue_infos, 0, sizeof(VkDeviceQueueCreateInfo) * VulkanDevice::max_queues);
    memset(queue_info_indices, -1, sizeof(int) * VulkanDevice::max_queues);

    u32 queue_family_count = 0;
    float queue_priorities[] = { 1.0f, 1.0f, 1.0f }; // in case all three queues are in the one family

    for (const auto& queue : device.queues)
    {
        if (queue_info_indices[queue.index] < 0)
        {
            queue_info_indices[queue.index] = queue_family_count++;

            auto& info = queue_infos[queue_info_indices[queue.index]];
            info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info.pNext = nullptr;
            info.flags = 0;
            info.queueFamilyIndex = queue.index;
            info.pQueuePriorities = queue_priorities;
            info.queueCount = 0;
        }

        ++queue_infos[queue_info_indices[queue.index]].queueCount;
    }

    VkPhysicalDeviceFeatures supported_features{};
    vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

    VkPhysicalDeviceFeatures enabled_features{};
    memset(&enabled_features, 0, sizeof(VkPhysicalDeviceFeatures));

#define BEE_ENABLE_FEATURE(vk_feature, bee_feature)                                                                 \
    BEE_BEGIN_MACRO_BLOCK                                                                                           \
        enabled_features.vk_feature = vkbool_cast(create_info.bee_feature && supported_features.vk_feature);        \
        if (create_info.bee_feature && vkbool_cast(enabled_features.vk_feature))                                    \
        {                                                                                                           \
            log_error(#bee_feature " is not a feature supported by the specified physical GPU device");             \
        }                                                                                                           \
    BEE_END_MACRO_BLOCK


    // Enable requested features if available
    BEE_ENABLE_FEATURE(depthClamp, enable_depth_clamp);
    BEE_ENABLE_FEATURE(sampleRateShading, enable_sample_rate_shading);
    BEE_ENABLE_FEATURE(samplerAnisotropy, enable_sampler_anisotropy);

#undef BEE_ENABLE_FEATURE

    VkDeviceCreateInfo device_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    device_info.queueCreateInfoCount = queue_family_count;
    device_info.pQueueCreateInfos = queue_infos;
    device_info.enabledExtensionCount = sign_cast<u32>(device_extensions.size());
    device_info.ppEnabledExtensionNames = device_extensions.data();
    device_info.pEnabledFeatures = &enabled_features;
    BEE_VK_CHECK(vkCreateDevice(physical_device, &device_info, nullptr, &device.handle));

    volkLoadDevice(device.handle); // register device with volk and load extensions

    VmaAllocatorCreateInfo vma_info{};
    vma_info.device = device.handle;
    vma_info.physicalDevice = physical_device;
    BEE_VK_CHECK(vmaCreateAllocator(&vma_info, &device.vma_allocator));

    return DeviceHandle(sign_cast<u32>(device_idx));
}

void gpu_destroy_device(const DeviceHandle& handle)
{
    auto& device = validate_device(handle);
    vmaDestroyAllocator(device.vma_allocator);
    vkDestroyDevice(device.handle, nullptr);
    device.handle = nullptr;
}

void gpu_device_wait(const DeviceHandle& handle)
{
    vkDeviceWaitIdle(validate_device(handle).handle);
}


} // namespace bee